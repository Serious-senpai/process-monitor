#include "fs.hpp"
#include "win32/path.hpp"
#include "win32/pch.hpp"

namespace _fs_impl
{
    NativeDirBuilder::NativeDirBuilder() : NonConstructible(NonConstructibleTag::TAG) {}

    io::Result<std::monostate> NativeDirBuilder::mkdir(const path::PathBuf &path) const
    {
        auto verbatim = get_long_path(path::PathBuf(path), true);
        OS_CVT(
            std::monostate,
            CreateDirectoryW(
                verbatim.is_ok() ? verbatim.unwrap().c_str() : path.c_str(),
                nullptr));
        return io::Result<std::monostate>::ok(std::monostate{});
    }
}
