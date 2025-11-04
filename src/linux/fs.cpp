#include "fs.hpp"
#include "linux/pch.hpp"

namespace _fs_impl
{
    io::Result<NativeMetadata> metadata(const path::PathBuf &path)
    {
        struct stat st = {};
        OS_CVT(NativeMetadata, ::stat(path.c_str(), &st));
        return io::Result<NativeMetadata>::ok(NativeMetadata(st));
    }
}
