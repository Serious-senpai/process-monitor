#include "error.hpp"

const Error *Error::source() const noexcept
{
    return nullptr;
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
