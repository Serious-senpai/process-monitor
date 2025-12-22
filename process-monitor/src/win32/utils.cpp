#include <deque>

#include <nlohmann/json.hpp>

#include "utils.hpp"
#include "generated/listener.hpp"

#include <tlhelp32.h>
#include <psapi.h>

using json = nlohmann::json;

bool stopped = false;

BOOL WINAPI _ctrl_handler(DWORD ctrl_type)
{
    if (ctrl_type == CTRL_C_EVENT || ctrl_type == CTRL_BREAK_EVENT)
    {
        std::cout << "\nShutting down..." << std::endl;
        stopped = true;
        return TRUE;
    }
    return FALSE;
}

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

class _CPUMetric
{
private:
    HANDLE _process;
    uint64_t _use;
    uint64_t _system;

    static std::pair<uint64_t, uint64_t> _get_process_and_system_time_ms(HANDLE process)
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

    uint64_t _cpu_usage(uint64_t new_use, uint64_t new_system)
    {
        if (new_system <= _system)
        {
            return 0;
        }

        // Return CPU usage with 3 decimal places
        auto result = (new_use - _use) * 100000 / (new_system - _system);
        _use = new_use;
        _system = new_system;
        return result;
    }

public:
    explicit _CPUMetric(HANDLE process) : _process(process), _use(0), _system(0)
    {
        auto [time_ms, system_ms] = _get_process_and_system_time_ms(process);
        _process = process;
        _use = time_ms;
        _system = system_ms;
    }

    uint64_t refresh()
    {
        auto [time_ms, system_ms] = _get_process_and_system_time_ms(_process);
        return _cpu_usage(time_ms, system_ms);
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

struct _ExitMetric
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
    HANDLE process;
    std::string name;
    Threshold threshold;
    _CPUMetric cpu;
    _MemoryMetric memory;
    _ExitMetric exit;

    explicit ProcessMetric(HANDLE process, const std::string &name)
        : process(process),
          name(name),
          cpu(process),
          memory(process),
          exit(process) {}
};

class _CTAContext
{
private:
    KernelTracerHandle *_tracer;
    uint16_t _port;
    std::unique_ptr<net::TcpStream> _stream;
    HANDLE _cpu_mem_thread;
    HANDLE _disk_network_thread;
    HANDLE _update_thread;

    CRITICAL_SECTION _reconnecting_cs;
    CONDITION_VARIABLE _reconnecting_cv;
    volatile LONG _reconnecting;

    CRITICAL_SECTION _queue_cs;
    CONDITION_VARIABLE _queue_cv;
    std::deque<procmon::ViolationInfo> _queue;

    CRITICAL_SECTION _monitored_pids_cs;
    std::unordered_map<uint64_t, ProcessMetric> _monitored_pids;

    static HANDLE _open_process(DWORD pid)
    {
        return OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, pid);
    }

    static DWORD _cpu_mem_loop(LPVOID param)
    {
        auto context = reinterpret_cast<_CTAContext *>(param);
        while (!stopped)
        {
            Sleep(1000);

            _CriticalSectionGuard guard(&context->_monitored_pids_cs);
            for (auto iter = context->_monitored_pids.begin(); iter != context->_monitored_pids.end() && !stopped;)
            {
                auto pid = iter->first;
                // std::cerr << "Measuring CPU/memory of process " << pid << std::endl;
                auto &metric = iter->second;

                auto cpu = metric.cpu.refresh();
                auto memory = metric.memory.memory_usage();

                if (metric.exit.exited())
                {
                    CloseHandle(metric.process);
                    iter = context->_monitored_pids.erase(iter);
                }
                else
                {
                    auto cpu_threshold = metric.threshold.values[static_cast<size_t>(Metric::Cpu)];
                    if (cpu >= cpu_threshold)
                    {
                        StaticCommandName name;
                        procmon::trim_command_name(metric.name.c_str(), &name);

                        Violation violation{Metric::Cpu, uint32_t(cpu), cpu_threshold};
                        context->push_violation(procmon::ViolationInfo(pid, std::move(name), std::move(violation)));
                    }

                    auto memory_threshold = metric.threshold.values[static_cast<size_t>(Metric::Memory)];
                    if (memory >= memory_threshold)
                    {
                        StaticCommandName name;
                        procmon::trim_command_name(metric.name.c_str(), &name);

                        Violation violation{Metric::Memory, uint32_t(memory), memory_threshold};
                        context->push_violation(procmon::ViolationInfo(pid, std::move(name), std::move(violation)));
                    }

                    iter++;
                }
            }
        }

        return ERROR_SUCCESS;
    }

    static DWORD _disk_network_loop(LPVOID param)
    {
        auto context = reinterpret_cast<_CTAContext *>(param);
        while (!stopped)
        {
            auto event = next_event(context->tracer(), 1000);
            // std::cerr << "Received event from tracer: " << event << std::endl;
            if (event != nullptr)
            {
                auto pid = event->pid;

                if (event->variant == EventType::NewProcess)
                {
                    HANDLE process = _open_process(pid);
                    if (process != NULL)
                    {
                        std::string name(reinterpret_cast<const char *>(event->name));

                        _CriticalSectionGuard guard(&context->_monitored_pids_cs);
                        context->_monitored_pids.emplace(pid, ProcessMetric(process, std::move(name)));
                    }
                    else
                    {
                        std::cerr << "Warning: Failed to open process " << pid << ": " << GetLastError() << std::endl;
                    }
                }
                else if (event->variant == EventType::Violation)
                {
                    context->push_violation(procmon::ViolationInfo(pid, event->name, std::move(event->data.violation)));
                }

                drop_event(event);
            }
        }

        return ERROR_SUCCESS;
    }

    static DWORD _update_loop(LPVOID param)
    {
        auto context = reinterpret_cast<_CTAContext *>(param);
        while (!stopped)
        {
            while (!stopped)
            {
                auto message = context->read_message();
                if (message.is_ok())
                {
                    auto &config = message.unwrap();
                    auto parsed = json::parse(config, nullptr, false);
                    if (!parsed.is_discarded() && parsed.is_array())
                    {
                        std::vector<procmon::ConfigEntry> entries;
                        for (const auto &item : parsed)
                        {
                            procmon::ConfigEntry entry = {0};

                            auto process = item.value("process", "");
                            size_t len = std::min(process.size(), COMMAND_LENGTH - 1);
                            std::memcpy(entry.name, process.data(), len);
                            entry.name[len] = '\0';

                            entry.threshold.values[static_cast<int>(Metric::Cpu)] = item.value("cpu", 0);
                            entry.threshold.values[static_cast<int>(Metric::Memory)] = item.value("memory", 0);
                            entry.threshold.values[static_cast<int>(Metric::Disk)] = item.value("disk", 0);
                            entry.threshold.values[static_cast<int>(Metric::Network)] = item.value("network", 0);
                            entries.push_back(entry);
                        }

                        context->set_monitor_targets(entries);
                    }
                    else
                    {
                        std::cerr << "Received corrupted data. Reconnecting." << std::endl;
                        break;
                    }
                }
                else
                {
                    // Unable to read incoming message, most likely the connection was closed
                    std::cerr << "Unable to pull update: " << message.unwrap_err().message() << std::endl;
                    context->reconnect();
                    break;
                }
            }

            // Sleep before retrying connection
            Sleep(5000);
        }

        return ERROR_SUCCESS;
    }

    void _populate_initial_processes(const std::vector<const char *> &targets)
    {
        // std::cerr << "Populating initial processes" << std::endl;
        _CriticalSectionGuard guard(&_monitored_pids_cs);
        auto processes = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
        if (processes != INVALID_HANDLE_VALUE)
        {
            PROCESSENTRY32W pe;
            pe.dwSize = sizeof(pe);

            std::vector<std::wstring> wtargets;
            for (auto &target : targets)
            {
                auto size = MultiByteToWideChar(CP_UTF8, 0, target, -1, nullptr, 0);
                std::wstring wtarget(size, 0);
                if (size > 0)
                {
                    MultiByteToWideChar(CP_UTF8, 0, target, -1, wtarget.data(), wtarget.size());
                }

                wtargets.push_back(std::move(wtarget));
            }

            if (Process32FirstW(processes, &pe))
            {
                do
                {
                    for (size_t i = 0; i < targets.size(); i++)
                    {
                        if (std::wcscmp(wtargets[i].c_str(), pe.szExeFile) == 0)
                        {
                            HANDLE process = _open_process(pe.th32ProcessID);
                            if (process != nullptr)
                            {
                                _monitored_pids.emplace(pe.th32ProcessID, ProcessMetric(process, targets[i]));
                            }
                        }
                    }
                } while (Process32NextW(processes, &pe));
            }

            CloseHandle(processes);
        }

        // std::cerr << "Done iterating all existing processes" << std::endl;
    }

public:
    explicit _CTAContext(KernelTracerHandle *tracer, uint16_t port, std::unique_ptr<net::TcpStream> stream)
        : _tracer(tracer), _port(port), _stream(std::move(stream)), _reconnecting(0)
    {
        InitializeCriticalSection(&_reconnecting_cs);
        InitializeConditionVariable(&_reconnecting_cv);
        InitializeCriticalSection(&_queue_cs);
        InitializeConditionVariable(&_queue_cv);
        InitializeCriticalSection(&_monitored_pids_cs);

        _cpu_mem_thread = CreateThread(nullptr, 0, _cpu_mem_loop, this, 0, nullptr);
        _disk_network_thread = CreateThread(nullptr, 0, _disk_network_loop, this, 0, nullptr);
        _update_thread = CreateThread(nullptr, 0, _update_loop, this, 0, nullptr);
    }

    static io::Result<std::unique_ptr<_CTAContext>> connect(uint16_t port)
    {
        auto tracer = new_tracer();
        if (tracer == nullptr)
        {
            return io::Result<std::unique_ptr<_CTAContext>>::err(io::Error::other("Failed to create kernel tracer"));
        }

        auto connect = net::TcpStream::connect(net::SocketAddrV4(net::Ipv4Addr::LOCALHOST, port));
        auto stream = connect.is_ok()
                          ? std::make_unique<net::TcpStream>(std::move(connect).into_ok())
                          : nullptr;
        auto context = std::make_unique<_CTAContext>(tracer, port, std::move(stream));
        if (context->_stream == nullptr)
        {
            std::cerr << "Loading configuration from local machine" << std::endl;
            auto load_from_local = procmon::load_config();
            if (load_from_local.is_ok())
            {
                std::cerr << "Loaded " << load_from_local.unwrap().size() << " configuration entries" << std::endl;
                context->set_monitor_targets(load_from_local.unwrap());
            }
            else
            {
                std::cerr << "Warning: Failed to load configuration from local machine: " << load_from_local.unwrap_err().message() << std::endl;
            }
        }

        return io::Result<std::unique_ptr<_CTAContext>>::ok(std::move(context));
    }

    ~_CTAContext()
    {
        WaitForSingleObject(_disk_network_thread, 1000);
        CloseHandle(_disk_network_thread);

        WaitForSingleObject(_cpu_mem_thread, 1000);
        CloseHandle(_cpu_mem_thread);

        WaitForSingleObject(_update_thread, 1000);
        CloseHandle(_update_thread);

        DeleteCriticalSection(&_monitored_pids_cs);
        DeleteCriticalSection(&_queue_cs);
        DeleteCriticalSection(&_reconnecting_cs);
        free_tracer(_tracer);
    }

    KernelTracerHandle *tracer() const
    {
        return _tracer;
    }

    io::Result<std::vector<char>> read_message()
    {
        if (_stream == nullptr)
        {
            return io::Result<std::vector<char>>::err(io::Error::other("Not connected to server"));
        }

        return procmon::read_message(*_stream);
    }

    io::Result<std::monostate> write_message(const std::span<const char> &buffer)
    {
        if (_stream == nullptr)
        {
            return io::Result<std::monostate>::err(io::Error::other("Not connected to server"));
        }

        uint32_t length = static_cast<uint32_t>(buffer.size());
        auto size = SHORT_CIRCUIT(std::monostate, _stream->write(std::span<const char>(reinterpret_cast<const char *>(&length), sizeof(length))));

        const char *ptr = buffer.data();
        auto remaining = buffer.size();
        while (remaining > 0)
        {
            size = SHORT_CIRCUIT(std::monostate, _stream->write(std::span<const char>(ptr, remaining)));
            ptr += size;
            remaining -= size;
        }

        return io::Result<std::monostate>::ok(std::monostate{});
    }

    void push_violation(procmon::ViolationInfo &&info)
    {
        _CriticalSectionGuard guard(&_queue_cs);
        // std::cerr << "Pushing violation for PID " << info.pid << std::endl;
        _queue.push_back(std::move(info));
        WakeConditionVariable(&_queue_cv);
    }

    void reconnect()
    {
        _CriticalSectionGuard guard(&_reconnecting_cs);
        while (!stopped && InterlockedCompareExchange(&_reconnecting, 1, 0) == 1)
        {
            SleepConditionVariableCS(&_reconnecting_cv, &_reconnecting_cs, 1000);
        }

        if (!stopped)
        {
            _stream = nullptr;

            auto connect = net::TcpStream::connect(net::SocketAddrV4(net::Ipv4Addr::LOCALHOST, _port));
            if (connect.is_ok())
            {
                _stream = std::make_unique<net::TcpStream>(std::move(connect).into_ok());
            }

            InterlockedExchange(&_reconnecting, 0);
            WakeAllConditionVariable(&_reconnecting_cv);
        }
    }

    void set_monitor_targets(const std::vector<procmon::ConfigEntry> &entries)
    {
        clear_monitor(_tracer);

        std::vector<const char *> targets;
        targets.reserve(entries.size());
        for (const auto &entry : entries)
        {
            auto target = reinterpret_cast<const char *>(entry.name);
            set_monitor(_tracer, target, &entry.threshold);
            targets.push_back(target);
        }

        procmon::save_config(entries);
        _populate_initial_processes(targets);
    }

    std::optional<procmon::ViolationInfo> next_violation()
    {
        _CriticalSectionGuard guard(&_queue_cs);
        while (!stopped && _queue.empty())
        {
            SleepConditionVariableCS(&_queue_cv, &_queue_cs, 1000);
        }

        if (!stopped)
        {
            auto info = _queue.front();
            _queue.pop_front();
            return info;
        }

        return std::nullopt;
    }
};

class _CTBContext
{
public:
    std::unique_ptr<net::TcpStream> stream;
    net::SocketAddr addr;

    explicit _CTBContext(
        std::unique_ptr<net::TcpStream> stream,
        net::SocketAddr &&addr,
        const std::string &json_config)
        : stream(std::move(stream)), addr(std::move(addr))
    {
        std::cerr << "Sending initial configuration to " << addr << std::endl;

        auto send = this->stream->write(std::span<const char>(json_config.data(), json_config.size()));
        if (send.is_err())
        {
            std::cerr << "Failed to send initial configuration to client: " << send.unwrap_err().message() << std::endl;
        }
    }
};

DWORD ctb_serve(LPVOID param)
{
    auto ctx = reinterpret_cast<_CTBContext *>(param);
    while (true)
    {
        auto message = procmon::read_message(*ctx->stream);
        if (message.is_ok())
        {
            auto info = reinterpret_cast<procmon::ViolationInfo *>(message.unwrap().data());
            std::cout << "Received violation from " << ctx->addr << ": PID=" << info->pid
                      << ", Process=" << info->name
                      << ", Metric=" << static_cast<int>(info->violation.metric)
                      << ", Value=" << info->violation.value
                      << ", Threshold=" << info->violation.threshold
                      << std::endl;
        }
        else
        {
            std::cerr << "Unable to receive messages from " << ctx->addr << ": " << message.unwrap_err().message() << std::endl;
            break;
        }
    }

    delete ctx;
    return ERROR_SUCCESS;
}

namespace procmon
{
    int cta_loop(uint16_t port)
    {
        initialize_logger(4);
        if (!SetConsoleCtrlHandler(_ctrl_handler, TRUE))
        {
            std::cerr << "Failed to set console control handler: " << GetLastError() << std::endl;
        }

        auto context_result = _CTAContext::connect(port);
        if (context_result.is_err())
        {
            std::cerr << "Failed to initialize context: " << context_result.unwrap_err().message() << std::endl;
            return 1;
        }

        auto context = std::move(context_result).into_ok();

        while (!stopped)
        {
            auto event = context->next_violation();
            if (event.has_value())
            {
                auto &violation = event.value();
                if (context->write_message(std::span<const char>(reinterpret_cast<const char *>(&violation), sizeof(violation))).is_err())
                {
                    context->reconnect();
                }
            }
        }

        return 0;
    }

    int ctb_loop(net::TcpListener &listener, const std::string &json_config)
    {
        while (!stopped)
        {
            // FIXME: Handle Ctrl-C
            auto client = listener.accept();
            std::cerr << "Accepted new client connection" << std::endl;
            if (client.is_ok())
            {
                auto pair = std::move(client).into_ok();
                auto stream = std::make_unique<net::TcpStream>(std::move(pair.first));
                auto ctx = new _CTBContext(std::move(stream), std::move(pair.second), json_config);
                CreateThread(nullptr, 0, ctb_serve, ctx, 0, nullptr);
            }
        }

        return 0;
    }
}
