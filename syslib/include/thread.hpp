#pragma once

#include "pch.hpp"

namespace thread
{
    template <typename Rep, typename Period>
    void sleep(const std::chrono::duration<Rep, Period> &duration)
    {
#ifdef _WIN32
#elif defined(__linux__)
        std::chrono::seconds std_seconds = std::chrono::floor<std::chrono::seconds>(duration);

        struct timespec time;
        time.tv_sec = std_seconds.count();
        time.tv_nsec = std::chrono::duration_cast<std::chrono::nanoseconds>(duration - std_seconds).count();

        nanosleep(&time, nullptr);
#endif
    }
}
