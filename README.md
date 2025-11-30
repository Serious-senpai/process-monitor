# process-monitor
Demonstrating Rust in Windows and Linux kernel-space

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
