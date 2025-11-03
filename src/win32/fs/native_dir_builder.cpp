#include "fs.hpp"
#include "win32/path.hpp"

namespace _fs_impl
{
    NativeDirBuilder::NativeDirBuilder() : NonConstructible(NonConstructibleTag::TAG) {}

    io::Result<std::monostate> NativeDirBuilder::mkdir(const path::PathBuf &path) const
    {
        if (CreateDirectoryW(path.c_str(), nullptr))
        {
            return io::Result<std::monostate>::ok(std::monostate{});
        }
        else
        {
            DWORD error = GetLastError();
            return io::Result<std::monostate>::err(
                io::IoError(io::IoErrorKind::Os, std::format("CreateDirectoryW: OS error {}", error)));
        }
    }
}
