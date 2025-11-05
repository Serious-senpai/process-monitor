#include "io.hpp"

namespace io
{
    Error Error::last_os_error()
    {
        return from_raw_os_error(GetLastError());
    }
}
