#include "fs.hpp"

OpenOptions::OpenOptions()
    : _read(false),
      _write(false),
      _append(false),
      _truncate(false),
      _create(false),
      _create_new(false)
#ifdef _WIN32
#elif defined(__linux__)
      ,
      _flags(0),
      _mode(0644)
#endif
{
}

OpenOptions &OpenOptions::read(bool read)
{
    _read = read;
    return *this;
}

OpenOptions &OpenOptions::write(bool write)
{
    _write = write;
    return *this;
}

OpenOptions &OpenOptions::append(bool append)
{
    _append = append;
    return *this;
}

OpenOptions &OpenOptions::truncate(bool truncate)
{
    _truncate = truncate;
    return *this;
}

OpenOptions &OpenOptions::create(bool create)
{
    _create = create;
    return *this;
}

OpenOptions &OpenOptions::create_new(bool create_new)
{
    _create_new = create_new;
    return *this;
}
