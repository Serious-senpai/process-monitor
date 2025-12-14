#include "cpu.hpp"

namespace procmon
{
    uint64_t get_cpus_count()
    {
        static uint64_t cpus = []
        {
            SYSTEM_INFO sysinfo;
            GetSystemInfo(&sysinfo);
            return sysinfo.dwNumberOfProcessors;
        }();

        return cpus;
    }
}
