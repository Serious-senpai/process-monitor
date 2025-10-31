#include "fs.hpp"

Result<int, IoError> OpenOptions::_get_access_mode() const
{
    int modes = (static_cast<int>(_read) << 2) | (static_cast<int>(_write) << 1) | static_cast<int>(_append);
    switch (modes)
    {
    case 0b100:
        return Result<int, IoError>::ok(O_RDONLY);
    case 0b010:
        return Result<int, IoError>::ok(O_WRONLY);
    case 0b110:
        return Result<int, IoError>::ok(O_RDWR);
    case 0b001:
    case 0b011:
        return Result<int, IoError>::ok(O_WRONLY | O_APPEND);
    case 0b101:
    case 0b111:
        return Result<int, IoError>::ok(O_RDWR | O_APPEND);
    default:
        if (_create || _create_new || _truncate)
        {
            return Result<int, IoError>::err(
                IoError(
                    IoErrorKind::InvalidInput,
                    "creating or truncating a file requires write or append access"));
        }
        else
        {
            return Result<int, IoError>::err(
                IoError(
                    IoErrorKind::InvalidInput,
                    "must specify at least one of read, write, or append access"));
        }
    }
}

Result<int, IoError> OpenOptions::_get_creation_mode() const
{
    int modes = (static_cast<int>(_write) << 1) | static_cast<int>(_append);
    switch (modes)
    {
    case 0b10:
        break;
    case 0b00:
        if (_truncate || _create || _create_new)
        {
            return Result<int, IoError>::err(
                IoError(
                    IoErrorKind::InvalidInput,
                    "creating or truncating a file requires write or append access"));
        }

        break;
    default:
        if (_truncate && !_create_new)
        {
            return Result<int, IoError>::err(
                IoError(
                    IoErrorKind::InvalidInput,
                    "creating or truncating a file requires write or append access"));
        }

        break;
    }

    modes = (static_cast<int>(_create) << 2) | (static_cast<int>(_truncate) << 1) | static_cast<int>(_create_new);
    switch (modes)
    {
    case 0b000:
        return Result<int, IoError>::ok(0);
    case 0b100:
        return Result<int, IoError>::ok(O_CREAT);
    case 0b010:
        return Result<int, IoError>::ok(O_TRUNC);
    case 0b110:
        return Result<int, IoError>::ok(O_CREAT | O_TRUNC);
    default:
        return Result<int, IoError>::ok(O_CREAT | O_EXCL);
    }
}

Result<size_t, IoError> File::read(std::span<char> buffer)
{
    if (buffer.empty())
    {
        return Result<size_t, IoError>::ok(0);
    }

    auto bytes = ::read(_fd, buffer.data(), buffer.size());
    if (bytes == -1)
    {
        return Result<size_t, IoError>::err(IoError(IoErrorKind::Os, std::format("OS error %d", errno)));
    }

    return Result<size_t, IoError>::ok(static_cast<size_t>(bytes));
}

Result<size_t, IoError> File::write(std::span<const char> buffer)
{
    if (buffer.empty())
    {
        return Result<size_t, IoError>::ok(0);
    }

    auto bytes = ::write(_fd, buffer.data(), buffer.size());
    if (bytes == -1)
    {
        return Result<size_t, IoError>::err(IoError(IoErrorKind::Os, std::format("OS error %d", errno)));
    }

    return Result<size_t, IoError>::ok(static_cast<size_t>(bytes));
}

Result<std::monostate, IoError> File::flush()
{
    if (::fsync(_fd) == -1)
    {
        return Result<std::monostate, IoError>::err(IoError(IoErrorKind::Os, std::format("OS error %d", errno)));
    }

    return Result<std::monostate, IoError>::ok(std::monostate{});
}
