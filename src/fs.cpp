#include "fs.hpp"

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

File::File(_NativeFile &&inner) : NonConstructible(NonConstructibleTag::TAG), _inner(std::move(inner)) {}

Result<class File, IoError> File::open(const char *path)
{
    OpenOptions options;
    return options.read(true).open(path);
}

Result<class File, IoError> File::create(const char *path)
{
    OpenOptions options;
    return options.write(true).create(true).truncate(true).open(path);
}

Result<class File, IoError> File::create_new(const char *path)
{
    OpenOptions options;
    return options.read(true).write(true).create_new(true).open(path);
}

Result<size_t, IoError> File::read(std::span<char> buffer)
{
    return _inner.read(buffer);
}

Result<size_t, IoError> File::write(std::span<const char> buffer)
{
    return _inner.write(buffer);
}

Result<std::monostate, IoError> File::flush()
{
    return _inner.flush();
}

Result<uint64_t, IoError> File::seek(SeekFrom position)
{
    return _inner.seek(position);
}
