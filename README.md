# process-monitor
Demonstrating Rust in Windows and Linux kernel-space

## Components

### CTA - Process Monitor Agent
CTA monitors resource usage of processes based on configuration. It uses:
- **Kernel tracer** (eBPF on Linux, ETW on Windows) for efficient disk and network I/O monitoring
- **OS APIs** for CPU and memory monitoring (GetProcessTimes/GetProcessMemoryInfo on Windows, /proc/pid/stat on Linux)
- **Persistent storage** for configuration (Registry on Windows, config file on Linux)

Features:
- Receives configuration from CTB via TCP
- Monitors multiple processes for CPU, memory, disk I/O, and network I/O thresholds
- Queues events when CTB is unavailable
- Low resource footprint (<5% CPU, <100MB memory)

### CTB - Process Monitor Log Server
CTB receives violation events from CTA and logs them to a file.

Features:
- Sends configuration to CTA as JSON
- Receives and logs violation events
- Interactive command interface for runtime configuration

## Configuration Format

```json
[
  {
    "process": "chrome.exe",
    "cpu": 10,
    "memory": 200,
    "disk": 1,
    "network": 500
  },
  {
    "process": "devenv.exe",
    "cpu": 50,
    "memory": 100,
    "disk": 10,
    "network": 100
  }
]
```

Fields:
- `process`: Process name to monitor
- `cpu`: CPU threshold in percent (0-100), **0 = disabled**
- `memory`: Memory threshold in MB, **0 = disabled**
- `disk`: Disk I/O threshold in MB/s, **0 = disabled**
- `network`: Network I/O threshold in KB/s, **0 = disabled**

**Note:** Setting a threshold to 0 disables monitoring for that resource type. To catch any usage, set the threshold to 1 (or another minimal value).

## Usage

Start CTB first (log server):
```bash
./CTB -c sample_config.json -l events.log
```

Then start CTA (monitoring agent):
```bash
./CTA
```

CTA will connect to CTB and receive the configuration. When processes exceed their configured thresholds, events are logged to the specified log file.

## Build instructions

Make sure to clone the repository with all submodules recursively via `git clone --recursive https://github.com/Serious-senpai/process-monitor`.

### Linux
Development inside an Ubuntu 24.04 VM is recommended. Run [`scripts/setup.sh`](/scripts/setup.sh) to automatically setup the build (and also development) environment.

### Windows
Prerequisites:
- [Windows Driver Kit (WDK)](https://learn.microsoft.com/en-us/windows-hardware/drivers/download-the-wdk)
- [Rust 1.91](https://releases.rs/docs/1.91.0)
- GNU C++ from [msys2](https://www.msys2.org)
- [CMake](https://cmake.org) either from [msys2](https://www.msys2.org) or [Visual Studio](https://learn.microsoft.com/en-us/cpp/build/cmake-projects-in-visual-studio)
