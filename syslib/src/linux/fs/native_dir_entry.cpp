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

    NativeDirEntry::NativeDirEntry(DIR *dir)
        : NonConstructible(NonConstructibleTag::TAG),
          _dir(dir),
          _entry(next_entry(dir))
    {
        _path = path::PathBuf(_entry->d_name);
    }

    NativeDirEntry::NativeDirEntry(NativeDirEntry &&other)
        : NonConstructible(NonConstructibleTag::TAG),
          _dir(other._dir),
          _entry(other._entry),
          _path(std::move(other._path))
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

    const path::PathBuf &NativeDirEntry::path() const noexcept
    {
        return _path;
    }

    io::Result<bool> NativeDirEntry::next()
    {
        errno = 0;
        _entry = next_entry(_dir);
        if (_entry == nullptr)
        {
            _path = path::PathBuf();
            if (errno == 0)
            {
                return io::Result<bool>::ok(false);
            }

            return io::Result<bool>::err(io::Error::last_os_error());
        }
        else
        {
            _path = path::PathBuf(_entry->d_name);
        }

        return io::Result<bool>::ok(true);
    }
}
