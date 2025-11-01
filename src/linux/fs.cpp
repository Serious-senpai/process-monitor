#include "fs.hpp"

_OpenOptions::_OpenOptions()
    : read(false),
      write(false),
      append(false),
      truncate(false),
      create(false),
      create_new(false),
      flags(0),
      mode(0644)
{
}

/// @see https://github.com/rust-lang/rust/blob/8182085617878610473f0b88f07fc9803f4b4960/library/std/src/sys/fs/unix.rs#L1125-L1148
Result<int, IoError> _OpenOptions::get_access_mode() const
{
    int modes = (static_cast<int>(read) << 2) | (static_cast<int>(write) << 1) | static_cast<int>(append);
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
        if (create || create_new || truncate)
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

/// @see https://github.com/rust-lang/rust/blob/8182085617878610473f0b88f07fc9803f4b4960/library/std/src/sys/fs/unix.rs#L1150-L1179
Result<int, IoError> _OpenOptions::get_creation_mode() const
{
    int modes = (static_cast<int>(write) << 1) | static_cast<int>(append);
    switch (modes)
    {
    case 0b10:
        break;
    case 0b00:
        if (truncate || create || create_new)
        {
            return Result<int, IoError>::err(
                IoError(
                    IoErrorKind::InvalidInput,
                    "creating or truncating a file requires write or append access"));
        }

        break;
    default:
        if (truncate && !create_new)
        {
            return Result<int, IoError>::err(
                IoError(
                    IoErrorKind::InvalidInput,
                    "creating or truncating a file requires write or append access"));
        }

        break;
    }

    modes = (static_cast<int>(create) << 2) | (static_cast<int>(truncate) << 1) | static_cast<int>(create_new);
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

_File::_File(int fd) : fd(fd), NonConstructible(NonConstructibleTag::TAG) {}

/// @see https://github.com/rust-lang/rust/blob/8182085617878610473f0b88f07fc9803f4b4960/library/std/src/sys/fs/unix.rs#L1199-L1214
Result<_File, IoError> _File::open(const char *path, const _OpenOptions &options)
{
    int flags = O_CLOEXEC |
                SHORT_CIRCUIT(_File, options.get_access_mode()) |
                SHORT_CIRCUIT(_File, options.get_creation_mode()) |
                options.flags;
    int fd = ::open(path, flags, options.mode);
    // TODO
}

Result<File, IoError> OpenOptions::open(const char *path)
{
    _File file = SHORT_CIRCUIT(File, _File::open(path, _inner));
    return Result<File, IoError>::ok(File(std::move(file)));
}

Result<size_t, IoError> File::read(std::span<char> buffer)
{
    if (buffer.empty())
    {
        return Result<size_t, IoError>::ok(0);
    }

    auto bytes = ::read(_inner.fd, buffer.data(), buffer.size());
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

    auto bytes = ::write(_inner.fd, buffer.data(), buffer.size());
    if (bytes == -1)
    {
        return Result<size_t, IoError>::err(IoError(IoErrorKind::Os, std::format("OS error %d", errno)));
    }

    return Result<size_t, IoError>::ok(static_cast<size_t>(bytes));
}

Result<std::monostate, IoError> File::flush()
{
    if (fsync(_inner.fd) == -1)
    {
        return Result<std::monostate, IoError>::err(IoError(IoErrorKind::Os, std::format("OS error %d", errno)));
    }

    return Result<std::monostate, IoError>::ok(std::monostate{});
}

Result<u_int64_t, IoError> File::seek(SeekFrom position)
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

    auto offset = lseek(_inner.fd, position.offset, whence);
    if (offset == -1)
    {
        return Result<u_int64_t, IoError>::err(IoError(IoErrorKind::Os, std::format("OS error %d", errno)));
    }

    return Result<u_int64_t, IoError>::ok(static_cast<u_int64_t>(offset));
}
