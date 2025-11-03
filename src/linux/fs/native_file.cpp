#include "fs.hpp"

NativeFile::NativeFile(int fd) : NonConstructible(NonConstructibleTag::TAG), _fd(fd) {}

NativeFile::NativeFile(NativeFile &&other) : NonConstructible(NonConstructibleTag::TAG)
{
    _fd = other._fd;
    other._fd = -1;
}

NativeFile::~NativeFile()
{
    if (_fd != -1)
    {
        close(_fd);
    }
}

Result<NativeFile, IoError> NativeFile::open(const char *path, const NativeOpenOptions &options)
{
    int flags = O_CLOEXEC |
                SHORT_CIRCUIT(NativeFile, options.get_access_mode()) |
                SHORT_CIRCUIT(NativeFile, options.get_creation_mode()) |
                options.flags;
    int fd = ::open(path, flags, options.mode);
    if (fd == -1)
    {
        return Result<NativeFile, IoError>::err(IoError(IoErrorKind::Os, std::format("OS error %d", errno)));
    }

    return Result<NativeFile, IoError>::ok(NativeFile(fd));
}

Result<size_t, IoError> NativeFile::read(std::span<char> buffer)
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

Result<size_t, IoError> NativeFile::write(std::span<const char> buffer)
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

Result<std::monostate, IoError> NativeFile::flush()
{
    if (fsync(_fd) == -1)
    {
        return Result<std::monostate, IoError>::err(IoError(IoErrorKind::Os, std::format("OS error %d", errno)));
    }

    return Result<std::monostate, IoError>::ok(std::monostate{});
}

Result<uint64_t, IoError> NativeFile::seek(SeekFrom position)
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
        return Result<uint64_t, IoError>::err(IoError(IoErrorKind::InvalidInput, "invalid seek type"));
    }

    auto offset = lseek(_fd, position.offset, whence);
    if (offset == -1)
    {
        return Result<uint64_t, IoError>::err(IoError(IoErrorKind::Os, std::format("OS error %d", errno)));
    }

    return Result<uint64_t, IoError>::ok(static_cast<uint64_t>(offset));
}
