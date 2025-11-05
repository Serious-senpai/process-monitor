#include "fs.hpp"

namespace _fs_impl
{
    NativeMetadata::NativeMetadata(struct stat stat) : NonConstructible(NonConstructibleTag::TAG), _stat(stat) {}

    NativeFileType NativeMetadata::file_type() const
    {
        return NativeFileType(_stat.st_mode);
    }

    bool NativeMetadata::is_dir() const
    {
        return file_type().is_dir();
    }

    bool NativeMetadata::is_file() const
    {
        return file_type().is_file();
    }

    bool NativeMetadata::is_symlink() const
    {
        return file_type().is_symlink();
    }
}
