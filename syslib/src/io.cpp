#include "io.hpp"

namespace io
{
    IoError::IoError(IoErrorKind kind, std::string &&message)
        : _kind(kind), _message(std::move(message)), NonConstructible(NonConstructibleTag::TAG) {}

    IoError IoError::other(std::string &&message)
    {
        return IoError(IoErrorKind::Other, std::move(message));
    }

    const char *IoError::message() const noexcept
    {
        return _message.c_str();
    }
}
