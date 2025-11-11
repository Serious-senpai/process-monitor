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

    int initialize_logger();
    int set_log_level(const int level);
    struct KernelTracerHandle *new_tracer();
    void free_tracer(struct KernelTracerHandle const *const tracer);
    int set_monitor(
        struct KernelTracerHandle const *const tracer,
        const char *const name,
        struct Threshold const *const threshold);
    int clear_monitor(struct KernelTracerHandle const *const tracer);
    struct Violation *next_event(
        struct KernelTracerHandle const *const tracer,
        const int timeout_ms);
    void drop_event(struct Violation *const event);

#ifdef __cplusplus
}
#endif
