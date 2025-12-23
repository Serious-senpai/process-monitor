#include "cpu.hpp"

namespace procmon
{
    uint64_t get_cpus_count()
    {
        static uint64_t cpus = sysconf(_SC_NPROCESSORS_ONLN);
        return cpus;
    }
}
