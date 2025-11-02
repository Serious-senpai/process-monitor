#include "fs.hpp"

_NativeFile::_NativeFile(int fd) : NonConstructible(NonConstructibleTag::TAG), _fd(fd) {}

_NativeFile::_NativeFile(_NativeFile &&other) : NonConstructible(NonConstructibleTag::TAG)
{
    _fd = other._fd;
    other._fd = -1;
}

_NativeFile::~_NativeFile()
{
    if (_fd != -1)
    {
        close(_fd);
    }
}

Result<_NativeFile, IoError> _NativeFile::open(const char *path, const _NativeOpenOptions &options)
{
    int flags = O_CLOEXEC |
                SHORT_CIRCUIT(_NativeFile, options.get_access_mode()) |
                SHORT_CIRCUIT(_NativeFile, options.get_creation_mode()) |
                options.flags;
    int fd = ::open(path, flags, options.mode);
    if (fd == -1)
    {
        return Result<_NativeFile, IoError>::err(IoError(IoErrorKind::Os, std::format("OS error %d", errno)));
    }

    return Result<_NativeFile, IoError>::ok(_NativeFile(fd));
}

Result<size_t, IoError> _NativeFile::read(std::span<char> buffer)
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

Result<size_t, IoError> _NativeFile::write(std::span<const char> buffer)
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

Result<std::monostate, IoError> _NativeFile::flush()
{
    if (fsync(_fd) == -1)
    {
        return Result<std::monostate, IoError>::err(IoError(IoErrorKind::Os, std::format("OS error %d", errno)));
    }

    return Result<std::monostate, IoError>::ok(std::monostate{});
}

Result<u_int64_t, IoError> _NativeFile::seek(SeekFrom position)
{
    int whence;
    switch (position.type)
    {
    case SeekFrom::Start:
        whence = SEEK_SET;
        break;
    case SeekFrom::Current:
        whence = SEEK_CUR;
        break;
    case SeekFrom::End:
        whence = SEEK_END;
        break;
    default:
        return Result<u_int64_t, IoError>::err(IoError(IoErrorKind::InvalidInput, "invalid seek type"));
    }

    auto offset = lseek(_fd, position.offset, whence);
    if (offset == -1)
    {
        return Result<u_int64_t, IoError>::err(IoError(IoErrorKind::Os, std::format("OS error %d", errno)));
    }

    return Result<u_int64_t, IoError>::ok(static_cast<u_int64_t>(offset));
}
