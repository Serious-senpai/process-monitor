#include "config.hpp"
#include "generated/listener.hpp"

#include <psapi.h>

class _CriticalSectionGuard
{
private:
    LPCRITICAL_SECTION _cs;

public:
    explicit _CriticalSectionGuard(LPCRITICAL_SECTION cs) : _cs(cs)
    {
        EnterCriticalSection(_cs);
    }

    ~_CriticalSectionGuard()
    {
        LeaveCriticalSection(_cs);
    }
};

std::pair<uint64_t, uint64_t> _get_process_and_system_time_ms(HANDLE process)
{
    FILETIME dummy, kernel_time, user_time;
    if (GetProcessTimes(process, &dummy, &dummy, &kernel_time, &user_time) == 0)
    {
        // std::cerr << "GetProcessTimes failed: " << GetLastError() << std::endl;
        return std::make_pair(0, 0);
    }

    uint64_t system_ms = GetTickCount64();

    ULARGE_INTEGER ktime;
    ktime.LowPart = kernel_time.dwLowDateTime;
    ktime.HighPart = kernel_time.dwHighDateTime;

    ULARGE_INTEGER utime;
    utime.LowPart = user_time.dwLowDateTime;
    utime.HighPart = user_time.dwHighDateTime;

    uint64_t time_ms = (ktime.QuadPart + utime.QuadPart) / 10000;
    return std::make_pair(time_ms, system_ms);
}

class _CPUMetric
{
private:
    HANDLE _process;
    uint64_t _use;
    uint64_t _system;

public:
    explicit _CPUMetric(HANDLE process) : _process(process), _use(0), _system(0)
    {
        auto [time_ms, system_ms] = _get_process_and_system_time_ms(process);
        _process = process;
        _use = time_ms;
        _system = system_ms;
    }

    uint64_t cpu_usage(uint64_t new_use, uint64_t new_system)
    {
        if (new_system <= _system)
        {
            return 0;
        }

        auto result = (new_use - _use) * 100000 / (new_system - _system);
        _use = new_use;
        _system = new_system;
        return result;
    }

    uint64_t refresh()
    {
        auto [time_ms, system_ms] = _get_process_and_system_time_ms(_process);
        return cpu_usage(time_ms, system_ms);
    }
};

class _MemoryMetric
{
private:
    HANDLE _process;

public:
    explicit _MemoryMetric(HANDLE process) : _process(process) {}

    uint64_t memory_usage() const
    {
        PROCESS_MEMORY_COUNTERS pmc;
        if (!GetProcessMemoryInfo(_process, &pmc, sizeof(pmc)))
        {
            // std::cerr << "GetProcessMemoryInfo failed: " << GetLastError() << std::endl;
            return 0;
        }

        return pmc.PagefileUsage;
    }
};

class _ExitMetric
{
private:
    HANDLE _process;

public:
    explicit _ExitMetric(HANDLE process) : _process(process) {}

    bool exited() const
    {
        DWORD exit_code = 0;
        if (!GetExitCodeProcess(_process, &exit_code))
        {
            return false;
        }

        return exit_code != STILL_ACTIVE;
    }
};

struct ProcessMetric
{
    _CPUMetric cpu;
    _MemoryMetric memory;
    _ExitMetric exit;
};

struct LoopContext
{
    CRITICAL_SECTION monitored_pids_cs;
    std::unordered_map<uint64_t, ProcessMetric> monitored_pids;
};

bool stopped = false;

DWORD loop(LPVOID param)
{
    auto context = static_cast<LoopContext *>(param);

    while (!stopped)
    {
        Sleep(1000);

        _CriticalSectionGuard guard(&context->monitored_pids_cs);
        for (auto iter = context->monitored_pids.begin(); iter != context->monitored_pids.end() && !stopped;)
        {
            auto pid = iter->first;
            auto &metric = iter->second;

            auto cpu = metric.cpu.refresh();
            auto memory = metric.memory.memory_usage();

            if (metric.exit.exited())
            {
                iter = context->monitored_pids.erase(iter);
            }
            else
            {
                std::cout << "PID " << pid << ": CPU " << static_cast<double>(cpu) / 1000.0 << "%, Memory " << memory / 1024 << " KB" << std::endl;
                iter++;
            }
        }
    }

    return 0;
}

int cta_main()
{
    initialize_logger(4);

    LoopContext context;
    InitializeCriticalSection(&context.monitored_pids_cs);

    auto tracer = new_tracer();
    if (tracer == nullptr)
    {
        std::cerr << "Failed to create tracer" << std::endl;
        return 1;
    }

    Threshold default_threshold;
    default_threshold.thresholds[0] = 0;
    default_threshold.thresholds[1] = 0;
    default_threshold.thresholds[2] = 0;
    default_threshold.thresholds[3] = 0;
    set_monitor(tracer, "curl.exe", &default_threshold);
    set_monitor(tracer, "notepad.exe", &default_threshold);

    auto thread = CreateThread(NULL, 0, loop, &context, 0, NULL);

    while (!stopped)
    {
        auto event = next_event(tracer, INFINITE);
        if (event != nullptr)
        {
            if (event->variant == EventType::NewProcess)
            {
                auto pid = event->pid;
                HANDLE process = OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, pid);
                if (process != NULL)
                {
                    _CriticalSectionGuard guard(&context.monitored_pids_cs);
                    context.monitored_pids.emplace(pid, ProcessMetric{_CPUMetric(process), _MemoryMetric(process), _ExitMetric(process)});
                    std::cout << "Started monitoring PID " << pid << std::endl;
                }
                else
                {
                    std::cerr << "Failed to open process " << pid << ": " << GetLastError() << std::endl;
                }
            }
            else if (event->variant == EventType::Violation)
            {
                std::cout << "PID " << event->pid << " violated threshold " << static_cast<int>(event->data.violation.metric) << ": " << event->data.violation.value << std::endl;
            }

            drop_event(event);
        }
    }

    WaitForSingleObject(thread, INFINITE);
    CloseHandle(thread);

    free_tracer(tracer);
    DeleteCriticalSection(&context.monitored_pids_cs);

    return 0;
}
