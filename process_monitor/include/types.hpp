#pragma once

#include "pch.hpp"

#ifdef __cplusplus
extern "C"
{
#endif

    struct Threshold
    {
        uint32_t threshold[4];
    };

    enum Metric : unsigned char
    {
        CPU = 0,
        MEMORY = 1,
        DISK = 2,
        NETWORK = 3
    };

    struct Violation
    {
        uint32_t pid;
        char name[16];
        Metric metric;
        uint32_t value;
        uint32_t threshold;
    };

    struct KernelTracerHandle;

#ifdef __cplusplus
}
#endif
