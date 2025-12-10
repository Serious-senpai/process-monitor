#pragma once

#include "io.hpp"

namespace procmon
{
    struct CPUMetric
    {
        uint64_t use;
        uint64_t system;

        uint64_t cpu_usage(uint64_t new_use, uint64_t new_system)
        {
            if (new_system <= system)
            {
                return 0;
            }

            auto result = (new_use - use) * 10000 / (new_system - system);
            use = new_use;
            system = new_system;
            return result;
        }
    };

    uint64_t get_cpus_count();
}
