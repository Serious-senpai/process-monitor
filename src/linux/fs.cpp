#include "fs.hpp"

Result<File, IoError> OpenOptions::open(const char *path)
{
    _NativeFile file = SHORT_CIRCUIT(File, _NativeFile::open(path, _inner));
    return Result<File, IoError>::ok(File(std::move(file)));
}
