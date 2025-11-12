#include "fs.hpp"

namespace _fs_impl
{
    NativeReadDir::NativeReadDir(path::PathBuf &&path)
        : NonConstructible(NonConstructibleTag::TAG), _path(std::move(path)) {}

    const path::PathBuf &NativeReadDir::path() const noexcept
    {
        return _path;
    }

    io::Result<NativeDirEntry> NativeReadDir::begin() const
    {
        return io::Result<NativeDirEntry>::ok(NativeDirEntry(opendir(_path.c_str())));
    }
}
