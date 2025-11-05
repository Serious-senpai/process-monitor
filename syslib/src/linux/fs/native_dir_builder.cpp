#include "fs.hpp"
#include "linux/pch.hpp"

namespace _fs_impl
{
    NativeDirBuilder::NativeDirBuilder() : NonConstructible(NonConstructibleTag::TAG), _mode(0777) {}

    io::Result<std::monostate> NativeDirBuilder::mkdir(const path::PathBuf &path) const
    {
        OS_CVT(std::monostate, ::mkdir(path.c_str(), _mode));
        return io::Result<std::monostate>::ok(std::monostate{});
    }

    void NativeDirBuilder::set_mode(mode_t mode) noexcept
    {
        _mode = mode;
    }
}
