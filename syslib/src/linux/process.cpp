#include "process.hpp"

namespace process
{
    uint32_t id()
    {
        return static_cast<uint32_t>(getpid());
    }
}
