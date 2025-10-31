#include "io.hpp"

IoError::IoError(IoErrorKind kind, std::string &&message)
    : _kind(kind), _message(std::move(message)) {}

const char *IoError::message() const noexcept
{
    return _message.c_str();
}
