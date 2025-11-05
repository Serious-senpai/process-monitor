#include "fs.hpp"

namespace _fs_impl
{
    NativeFileType::NativeFileType(DWORD attributes, DWORD reparse_tag)
        : NonConstructible(NonConstructibleTag::TAG),
          _is_dir(attributes & FILE_ATTRIBUTE_DIRECTORY),
          _is_symlink((attributes & FILE_ATTRIBUTE_REPARSE_POINT) && (reparse_tag & 0x20000000)) {}

    bool NativeFileType::is_dir() const
    {
        return !_is_symlink && _is_dir;
    }

    bool NativeFileType::is_file() const
    {
        return !_is_symlink && !_is_dir;
    }

    bool NativeFileType::is_symlink() const
    {
        return _is_symlink;
    }

    bool NativeFileType::is_symlink_dir() const
    {
        return _is_symlink && _is_dir;
    }

    bool NativeFileType::is_symlink_file() const
    {
        return _is_symlink && !_is_dir;
    }
}
