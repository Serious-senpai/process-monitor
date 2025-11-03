#include "fs.hpp"

namespace fs
{
    OpenOptions::OpenOptions() : _inner() {}

    OpenOptions &OpenOptions::read(bool read)
    {
        _inner.read = read;
        return *this;
    }

    OpenOptions &OpenOptions::write(bool write)
    {
        _inner.write = write;
        return *this;
    }

    OpenOptions &OpenOptions::append(bool append)
    {
        _inner.append = append;
        return *this;
    }

    OpenOptions &OpenOptions::truncate(bool truncate)
    {
        _inner.truncate = truncate;
        return *this;
    }

    OpenOptions &OpenOptions::create(bool create)
    {
        _inner.create = create;
        return *this;
    }

    OpenOptions &OpenOptions::create_new(bool create_new)
    {
        _inner.create_new = create_new;
        return *this;
    }

    io::Result<File> OpenOptions::open(const char *path)
    {
        auto file = SHORT_CIRCUIT(File, _fs_impl::NativeFile::open(path, _inner));
        return io::Result<File>::ok(File(std::move(file)));
    }

    File::File(_fs_impl::NativeFile &&inner) : NonConstructible(NonConstructibleTag::TAG), _inner(std::move(inner)) {}

    io::Result<File> File::open(const char *path)
    {
        OpenOptions options;
        return options.read(true).open(path);
    }

    io::Result<File> File::create(const char *path)
    {
        OpenOptions options;
        return options.write(true).create(true).truncate(true).open(path);
    }

    io::Result<File> File::create_new(const char *path)
    {
        OpenOptions options;
        return options.read(true).write(true).create_new(true).open(path);
    }

    io::Result<size_t> File::read(std::span<char> buffer)
    {
        return _inner.read(buffer);
    }

    io::Result<size_t> File::write(std::span<const char> buffer)
    {
        return _inner.write(buffer);
    }

    io::Result<std::monostate> File::flush()
    {
        return _inner.flush();
    }

    io::Result<uint64_t> File::seek(io::SeekFrom position)
    {
        return _inner.seek(position);
    }
}
