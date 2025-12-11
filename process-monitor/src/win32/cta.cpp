#include "config.hpp"
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

LoopContext CTA_CONTEXT;

void loop()
{
    InitializeCriticalSection(&CTA_CONTEXT.monitored_pids_cs);

    while (true)
    {
        Sleep(1000);

        _CriticalSectionGuard guard(&CTA_CONTEXT.monitored_pids_cs);
        for (auto iter = CTA_CONTEXT.monitored_pids.begin(); iter != CTA_CONTEXT.monitored_pids.end();)
        {
            auto pid = iter->first;
            auto &metric = iter->second;

            auto cpu = metric.cpu.refresh();
            auto memory = metric.memory.memory_usage();

            if (metric.exit.exited())
            {
                iter = CTA_CONTEXT.monitored_pids.erase(iter);
            }
            else
            {
                std::cout << "PID " << pid << ": CPU " << static_cast<double>(cpu) / 1000.0 << "%, Memory " << memory / 1024 << " KB" << std::endl;
                iter++;
            }
        }
    }

    DeleteCriticalSection(&CTA_CONTEXT.monitored_pids_cs);
}

int cta_main()
{
    auto process = OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, 15756);
    CTA_CONTEXT.monitored_pids.emplace(
        15756,
        ProcessMetric{
            _CPUMetric(process),
            _MemoryMetric(process),
            _ExitMetric(process)});

    loop();

    return 0;
}
