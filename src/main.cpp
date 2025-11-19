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

#ifdef __linux__
class Statistics
{
private:
    uint64_t _cpu;
    uint64_t _rss;
    uint64_t _disk;
    uint64_t _total;

    uint64_t _delta_rate(uint64_t stats, uint64_t old_stats, const Statistics &old) const
    {
        auto ticks = _total - old._total;
        if (ticks == 0)
        {
            return 0;
        }

        static uint64_t cpu_cores = sysconf(_SC_NPROCESSORS_ONLN);

        // Multiply by 100 to get percentage, another 100 to retain two decimal places
        return (stats - old_stats) * 10000 * cpu_cores / ticks;
    }

public:
    explicit Statistics(uint64_t cpu, uint64_t rss, uint64_t disk, uint64_t total)
        : _cpu(cpu), _rss(rss), _disk(disk), _total(total) {}

    uint64_t cpu_usage(const Statistics &old) const
    {
        return _delta_rate(_cpu, old._cpu, old);
    }

    uint64_t memory_usage() const
    {
        static uint64_t page_size = sysconf(_SC_PAGESIZE);
        return _rss * page_size;
    }

    uint64_t disk_usage(const Statistics &old) const
    {
        static uint64_t user_hz = sysconf(_SC_CLK_TCK);
        return _delta_rate(_disk, old._disk, old) / user_hz;
    }
};

std::atomic<bool> stopped = false;

std::mutex pids_mutex;
std::unordered_map<uint32_t, Statistics> pids;

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

uint64_t get_disk_usage(uint32_t pid, std::span<char> buffer)
{
    auto file = fs::File::open(std::format("/proc/{}/io", pid));

    if (file.is_ok())
    {
        auto size = file.unwrap().read(buffer);
        if (size.is_ok())
        {
            buffer[size.unwrap()] = '\0';
            std::istringstream stream(buffer.data());

            std::string ignore;
            for (int i = 0; i < 9; i++)
            {
                stream >> ignore;
            }

            uint64_t read_bytes, write_bytes;
            stream >> read_bytes >> ignore >> write_bytes;

            return read_bytes + write_bytes;
        }
    }

    return 0;
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

            auto disk = get_disk_usage(pid, span);
            auto [cpu, rss] = get_cpu_mem_usage<false>(pid, span);
            if (cpu > 0 || rss > 0)
            {
                pids.emplace(pid, Statistics(cpu, rss, disk, get_total_cpu_time(span)));
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

void detect_cpu_mem_disk()
{
    char buffer[512] = {0};
    std::span<char> span(buffer, sizeof(buffer));

    populate_initial_pids();

    while (!stopped.load())
    {
        std::lock_guard<std::mutex> lock(pids_mutex);
        for (auto iter = pids.begin(); iter != pids.end();)
        {
            auto disk = get_disk_usage(iter->first, span);
            auto [cpu, rss] = get_cpu_mem_usage<true>(iter->first, span);

            if (cpu > 0 || rss > 0)
            {
                Statistics new_state(cpu, rss, disk, get_total_cpu_time(span));
                auto cpu_usage = new_state.cpu_usage(iter->second);
                auto mem_usage = new_state.memory_usage();
                auto disk_usage = new_state.disk_usage(iter->second);
                iter->second = new_state;

                std::cout << "PID " << iter->first << " CPU usage: " << static_cast<double>(cpu_usage) / 100.0
                          << " %, Memory usage: " << mem_usage
                          << " bytes, Disk usage: " << static_cast<double>(disk_usage) / 100.0 << " B/s" << std::endl;
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

    std::thread cpu_thread(detect_cpu_mem_disk);
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

                auto disk = get_disk_usage(event->pid, span);
                auto [cpu, rss] = get_cpu_mem_usage<true>(event->pid, span);
                if (cpu > 0 || rss > 0)
                {
                    pids.emplace(event->pid, Statistics(cpu, rss, disk, get_total_cpu_time(span)));
                }
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

#elif defined(_WIN32)

int main()
{
    if (initialize_logger(3))
    {
        std::cerr << "Failed to initialize logger." << std::endl;
        return 1;
    }

    auto tracer = new_tracer();
    if (tracer == nullptr)
    {
        std::cerr << "Failed to create tracer." << std::endl;
        return 1;
    }

    free_tracer(tracer);
}

#endif
