#include "fs.hpp"

namespace _fs_impl
{
    NativeMetadata::NativeMetadata(WIN32_FIND_DATAW &&data)
        : NonConstructible(NonConstructibleTag::TAG), _data(std::move(data)) {}

    bool NativeMetadata::is_dir() const
    {
        return _data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY;
    }

    bool NativeMetadata::is_file() const
    {
        return !is_dir() && !is_symlink();
    }

    bool NativeMetadata::is_symlink() const
    {
        return _data.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT;
    }
}
