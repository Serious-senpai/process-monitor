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

File::File(_File &&inner) : _inner(std::move(inner)), NonConstructible(NonConstructibleTag::TAG) {}

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
