#include "fs.hpp"

namespace _fs_impl
{
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

    io::Result<NativeFile> NativeFile::open(const path::PathBuf &path, const NativeOpenOptions &options)
    {
        int flags = O_CLOEXEC |
                    SHORT_CIRCUIT(NativeFile, options.get_access_mode()) |
                    SHORT_CIRCUIT(NativeFile, options.get_creation_mode()) |
                    options.flags;
        int fd = ::open(path.c_str(), flags, options.mode);
        if (fd == -1)
        {
            return io::Result<NativeFile>::err(io::Error::last_os_error());
        }

        return io::Result<NativeFile>::ok(NativeFile(fd));
    }

    io::Result<size_t> NativeFile::read(std::span<char> buffer)
    {
        if (buffer.empty())
        {
            return io::Result<size_t>::ok(0);
        }

        auto bytes = ::read(_fd, buffer.data(), buffer.size());
        if (bytes == -1)
        {
            return io::Result<size_t>::err(io::Error::last_os_error());
        }

        return io::Result<size_t>::ok(static_cast<size_t>(bytes));
    }

    io::Result<size_t> NativeFile::write(std::span<const char> buffer)
    {
        if (buffer.empty())
        {
            return io::Result<size_t>::ok(0);
        }

        auto bytes = ::write(_fd, buffer.data(), buffer.size());
        if (bytes == -1)
        {
            return io::Result<size_t>::err(io::Error::last_os_error());
        }

        return io::Result<size_t>::ok(static_cast<size_t>(bytes));
    }

    io::Result<std::monostate> NativeFile::flush()
    {
        if (fsync(_fd) == -1)
        {
            return io::Result<std::monostate>::err(io::Error::last_os_error());
        }

        return io::Result<std::monostate>::ok(std::monostate{});
    }

    io::Result<uint64_t> NativeFile::seek(io::SeekFrom position)
    {
        int whence;
        switch (position.type)
        {
        case io::SeekFrom::Start:
            whence = SEEK_SET;
            break;
        case io::SeekFrom::Current:
            whence = SEEK_CUR;
            break;
        case io::SeekFrom::End:
            whence = SEEK_END;
            break;
        default:
            return io::Result<uint64_t>::err(io::Error(io::ErrorKind::InvalidInput, "invalid seek type"));
        }

        auto offset = lseek(_fd, position.offset, whence);
        if (offset == -1)
        {
            return io::Result<uint64_t>::err(io::Error::last_os_error());
        }

        return io::Result<uint64_t>::ok(static_cast<uint64_t>(offset));
    }
}
