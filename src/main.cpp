#include <atomic>
#include <iostream>
#include <mutex>
#include <thread>
#include <unordered_set>

#include "fs.hpp"
#include "io.hpp"
#include "listener.hpp"

std::atomic<bool> stopped = false;
std::mutex monitor_mutex;
std::unordered_set<uint32_t> monitored_pids;

void detect_cpu()
{
    while (!stopped.load())
    {
        std::lock_guard<std::mutex> lock(monitor_mutex);
        for (auto iter = monitored_pids.begin(); iter != monitored_pids.end();)
        {
            auto pid = *iter;
            std::cout << "Checking " << pid << std::endl;
            auto file = fs::File::open(std::format("/proc/{}/stat", pid));
            std::cout << "Opened /proc/" << pid << "/stat" << std::endl;

            bool exists = true;
            if (file.is_ok())
            {
                // TODO: Check that the command name is actually the one we expect (via `/proc/[pid]/comm`)
                // TODO: Measure CPU and memory
                char buffer[1024];
                auto read_result = file.unwrap().read(std::span<char>(buffer, sizeof(buffer) - 1));
                std::cout << "Process " << pid << " is alive." << std::endl;
                exists = true;
            }
            else
            {
                if (file.unwrap_err().kind() == io::ErrorKind::NotFound)
                {
                    std::cout << "Process " << pid << " has exited." << std::endl;
                    exists = false;
                }
                else
                {
                    exists = true;
                }
            }

            if (exists)
            {
                iter++;
            }
            else
            {
                iter = monitored_pids.erase(iter);
            }
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }
}

int main(int argc, char **argv)
{
    if (initialize_logger(4))
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
    }

    std::thread cpu_thread(detect_cpu);
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

                std::lock_guard<std::mutex> lock(monitor_mutex);
                monitored_pids.insert(event->pid);
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
