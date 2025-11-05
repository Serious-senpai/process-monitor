#include "io.hpp"

namespace io
{
    Error::Error(ErrorKind kind, std::string &&message)
        : _kind(kind), _message(std::move(message)), NonConstructible(NonConstructibleTag::TAG) {}

    Error Error::other(std::string &&message)
    {
        return Error(ErrorKind::Other, std::move(message));
    }

    Error Error::from_raw_os_error(int code)
    {
        // TODO
    }

    const char *Error::message() const noexcept
    {
        return _message.c_str();
    }
}
