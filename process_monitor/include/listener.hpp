#pragma once

#include "types.hpp"

#ifdef __cplusplus
extern "C"
{
#endif

    int initialize_logger(const int level);
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
