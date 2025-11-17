#include <atomic>
#include <iostream>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <unordered_set>

#include "fs.hpp"
#include "io.hpp"
#include "listener.hpp"

struct CPUStat
{
    uint64_t usage;
    uint64_t total;

    uint64_t rate(const CPUStat &old) const
    {
        if (total == old.total)
        {
            return 0;
        }

        return (usage - old.usage) * 10000 / (total - old.total);
    }
};

std::atomic<bool> stopped = false;

std::mutex pids_mutex;
std::unordered_map<uint32_t, CPUStat> pids;

std::mutex names_mutex;
std::unordered_set<std::string> names;

bool is_numeric(const std::string &s)
{
    return !s.empty() && std::all_of(s.begin(), s.end(), isdigit);
}

template <bool ACQ_NAMES_MUTEX>
std::pair<uint64_t, uint64_t> get_cpu_mem_usage(uint32_t pid, std::span<char> buffer)
{
    auto file = fs::File::open(std::format("/proc/{}/stat", pid));

    if (file.is_ok())
    {
        auto size = file.unwrap().read(buffer);
        if (size.is_ok())
        {
            buffer[size.unwrap()] = '\0';

            auto ptr = buffer.data();
            while (*ptr != '(' && ptr - buffer.data() + 1 < size.unwrap())
            {
                ptr++;
            }

            std::istringstream stream(ptr + 1); // skip '('
            std::string comm;
            stream >> comm;
            comm.pop_back(); // remove trailing ')'

            {
                if constexpr (ACQ_NAMES_MUTEX)
                {
                    std::lock_guard<std::mutex> lock(names_mutex);
                }

                if (names.find(comm) == names.end())
                {
                    return std::make_pair(0, 0);
                }
            }

            // Currently at field 3 (1-indexed), skip until field 14
            for (int i = 3; i < 14; i++)
            {
                stream >> comm;
            }

            // Read fields 14, 15
            uint64_t utime, stime;
            stream >> utime >> stime;

            // Currently at field 16, skip until field 24
            for (int i = 16; i < 24; i++)
            {
                stream >> comm;
            }

            uint64_t rss;
            stream >> rss;

            return std::make_pair(utime + stime, rss);
        }
    }

    return std::make_pair(0, 0);
}

uint64_t get_total_cpu_time(std::span<char> buffer)
{
    auto file = fs::File::open("/proc/stat");

    if (file.is_ok())
    {
        auto size = file.unwrap().read(buffer);
        if (size.is_ok())
        {
            buffer[size.unwrap()] = '\0';
            std::istringstream stream(buffer.data());
            std::string cpu_label;
            stream >> cpu_label; // skip "cpu" label

            uint64_t user, nice, system, idle, iowait, irq, softirq, steal;
            stream >> user >> nice >> system >> idle >> iowait >> irq >> softirq >> steal;

            return user + nice + system + idle + iowait + irq + softirq + steal;
        }
    }

    return 0;
}

void populate_initial_pids()
{
    char buffer[512] = {0};
    std::span<char> span(buffer, sizeof(buffer));

    std::lock_guard<std::mutex> lock(pids_mutex), lock2(names_mutex);
    auto dir = fs::read_dir("/proc");

    auto entry = dir.begin();
    if (entry.is_err())
    {
        std::cerr << "Failed to read /proc directory: " << entry.unwrap_err().message() << std::endl;
        return;
    }

    auto direntry = std::move(entry).into_ok();
    while (true)
    {
        auto &path = direntry.path();
        auto filename = path.filename().string();

        if (is_numeric(filename))
        {
            uint32_t pid = std::stoul(filename);

            auto cpu = get_cpu_mem_usage<false>(pid, span).first;
            if (cpu > 0)
            {
                pids[pid] = CPUStat{cpu, get_total_cpu_time(span)};
            }
        }

        auto has_next = direntry.next();
        if (has_next.is_ok() && !has_next.unwrap())
        {
            break;
        }

        if (has_next.is_err())
        {
            std::cerr << "Failed to read next /proc entry: " << has_next.unwrap_err().message() << std::endl;
            break;
        }
    }
}

void detect_cpu_mem()
{
    char buffer[512] = {0};
    std::span<char> span(buffer, sizeof(buffer));

    populate_initial_pids();

    while (!stopped.load())
    {
        std::lock_guard<std::mutex> lock(pids_mutex);
        for (auto iter = pids.begin(); iter != pids.end();)
        {
            auto [cpu_usage, mem_usage] = get_cpu_mem_usage<true>(iter->first, span);

            if (cpu_usage > 0 || mem_usage > 0)
            {
                CPUStat new_state{cpu_usage, get_total_cpu_time(span)};
                auto rate = new_state.rate(iter->second);
                iter->second = new_state;

                std::cout << "PID " << iter->first << " CPU usage: " << static_cast<double>(rate) / 100.0 << "%" << ", Memory usage: " << mem_usage << std::endl;
                iter++;
            }
            else
            {
                std::cout << "PID " << iter->first << " has exited monitoring." << std::endl;
                iter = pids.erase(iter);
            }
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }
}

int main(int argc, char **argv)
{
    if (initialize_logger(3))
    {
        std::cerr << "Failed to initialize logger." << std::endl;
        return 1;
    }

    auto tracer = new_tracer();
    if (tracer == nullptr)
    {
        std::cerr << "Failed to create tracer. Are you root?" << std::endl;
        return 1;
    }

    Threshold threshold = {};
    threshold.thresholds[static_cast<size_t>(Metric::Disk)] = threshold.thresholds[static_cast<size_t>(Metric::Network)] = 0;
    for (int i = 1; i < argc; i++)
    {
        if (set_monitor(tracer, argv[i], &threshold))
        {
            std::cerr << "Failed to set monitor for " << argv[i] << std::endl;
            free_tracer(tracer);
            return 1;
        }

        names.emplace(argv[i]);
    }

    char buffer[512] = {0};
    std::span<char> span(buffer, sizeof(buffer));

    std::thread cpu_thread(detect_cpu_mem);
    while (true)
    {
        auto event = next_event(tracer, -1);
        if (event != nullptr)
        {
            if (event->variant == EventType::Violation)
            {
                std::cout << "Violation Event detected: PID " << event->pid
                          << ", Name: " << reinterpret_cast<const char *>(event->name)
                          << ", Metric: " << static_cast<int>(event->data.violation.metric)
                          << ", Value: " << event->data.violation.value
                          << ", Threshold: " << event->data.violation.threshold
                          << std::endl;
            }
            else if (event->variant == EventType::NewProcess)
            {
                std::cout << "New Process Event detected: PID " << event->pid
                          << ", Name: " << reinterpret_cast<const char *>(event->name)
                          << std::endl;

                std::lock_guard<std::mutex> lock(pids_mutex);
                pids[event->pid] = CPUStat{get_cpu_mem_usage<true>(event->pid, span).first, get_total_cpu_time(span)};
            }

            drop_event(event);
        }
        else
        {
            std::cout << "No event received." << std::endl;
        }
    }

    free_tracer(tracer);

    return 0;
}
