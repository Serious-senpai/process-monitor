#include "fs.hpp"

namespace _fs_impl
{
    NativeFileType::NativeFileType(mode_t mode) : NonConstructible(NonConstructibleTag::TAG), _mode(mode) {}

    bool NativeFileType::is_dir() const
    {
        return is(S_IFDIR);
    }

    bool NativeFileType::is_file() const
    {
        return is(S_IFREG);
    }

    bool NativeFileType::is_symlink() const
    {
        return is(S_IFLNK);
    }

    bool NativeFileType::is(mode_t mode) const
    {
        return (_mode & S_IFMT) == mode;
    }
}
