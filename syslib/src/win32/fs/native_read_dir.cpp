#include "fs.hpp"
#include "win32/path.hpp"

namespace _fs_impl
{
    NativeReadDir::NativeReadDir(path::PathBuf &&path) : NonConstructible(NonConstructibleTag::TAG), _path(std::move(path)) {}

    const path::PathBuf &NativeReadDir::path() const noexcept
    {
        return _path;
    }

    io::Result<NativeDirEntry> NativeReadDir::begin() const
    {
        if (_path.empty())
        {
            return io::Result<NativeDirEntry>::err(io::Error::from_raw_os_error(ERROR_PATH_NOT_FOUND));
        }

        auto long_path = SHORT_CIRCUIT(NativeDirEntry, get_long_path(_path / "*", true));

        WIN32_FIND_DATAW wfd;
        auto dir = FindFirstFileExW(
            long_path.c_str(),
            FindExInfoBasic,
            &wfd,
            FindExSearchNameMatch,
            nullptr,
            0);

        if (dir != INVALID_HANDLE_VALUE)
        {
            return io::Result<NativeDirEntry>::ok(NativeDirEntry(dir, wfd));
        }

        auto error = GetLastError();
        if (error == ERROR_FILE_NOT_FOUND)
        {
            return io::Result<NativeDirEntry>::ok(NativeDirEntry(INVALID_HANDLE_VALUE, std::nullopt));
        }

        return io::Result<NativeDirEntry>::err(io::Error::from_raw_os_error(error));
    }
}
