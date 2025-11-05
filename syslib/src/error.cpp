#include "error.hpp"

namespace error
{
    const Error *Error::source() const noexcept
    {
        return nullptr;
    }

    const char *Error::what() const noexcept
    {
        return message();
    }

    std::ostream &operator<<(std::ostream &os, const Error &err)
    {
        os << err.message();
        const Error *src = err.source();
        if (src)
        {
            os << " [caused by: " << *src << "]";
        }
        return os;
    }
}
