#include "fs.hpp"

namespace _fs_impl
{
    NativeDirEntry::NativeDirEntry(HANDLE dir, std::optional<WIN32_FIND_DATAW> entry)
        : NonConstructible(NonConstructibleTag::TAG), _dir(dir), _entry(entry)
    {
        if (entry.has_value())
        {
            _path = path::PathBuf(entry->cFileName);
        }
        else
        {
            _path = path::PathBuf();
        }
    }

    NativeDirEntry::NativeDirEntry(NativeDirEntry &&other)
        : NonConstructible(NonConstructibleTag::TAG),
          _dir(other._dir),
          _entry(other._entry),
          _path(std::move(other._path))
    {
        other._dir = INVALID_HANDLE_VALUE;
        other._entry = std::nullopt;
    }

    NativeDirEntry::~NativeDirEntry()
    {
        if (_dir != INVALID_HANDLE_VALUE)
        {
            FindClose(_dir);
        }
    }

    const path::PathBuf &NativeDirEntry::path() const noexcept
    {
        return _path;
    }

    io::Result<bool> NativeDirEntry::next()
    {
        if (_dir == INVALID_HANDLE_VALUE || _entry == std::nullopt)
        {
            return io::Result<bool>::ok(false);
        }

        if (FindNextFileW(_dir, &*_entry))
        {
            _path = path::PathBuf(_entry->cFileName);
            return io::Result<bool>::ok(true);
        }
        else
        {
            // If `FindNextFileW` fails, the `_entry` will be indeterminate.
            _entry = std::nullopt;
            _path = path::PathBuf();

            const auto error = GetLastError();
            if (error == ERROR_NO_MORE_FILES)
            {
                return io::Result<bool>::ok(false);
            }
            else
            {
                return io::Result<bool>::err(io::Error::from_raw_os_error(error));
            }
        }
    }
}
