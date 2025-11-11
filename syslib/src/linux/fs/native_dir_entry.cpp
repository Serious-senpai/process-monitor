#include "fs.hpp"

namespace _fs_impl
{
    dirent *next_entry(DIR *dir)
    {
        if (dir == nullptr)
        {
            return nullptr;
        }

        return readdir(dir);
    }

    NativeDirEntry::NativeDirEntry(DIR *dir) : NonConstructible(NonConstructibleTag::TAG), _dir(dir), _entry(next_entry(dir)) {}
    NativeDirEntry::NativeDirEntry(NativeDirEntry &&other)
        : NonConstructible(NonConstructibleTag::TAG), _dir(other._dir), _entry(other._entry)
    {
        other._dir = nullptr;
        other._entry = nullptr;
    }

    NativeDirEntry::~NativeDirEntry()
    {
        _entry = nullptr;
        if (_dir != nullptr)
        {
            closedir(_dir);
            _dir = nullptr;
        }
    }

    io::Result<bool> NativeDirEntry::next()
    {
        errno = 0;
        _entry = next_entry(_dir);
        if (_entry == nullptr)
        {
            if (errno == 0)
            {
                return io::Result<bool>::ok(false);
            }

            return io::Result<bool>::err(io::Error::last_os_error());
        }

        return io::Result<bool>::ok(true);
    }
}
