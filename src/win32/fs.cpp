#include "fs.hpp"

namespace _fs_impl
{
    io::Result<NativeMetadata> _metadata(const path::PathBuf &path, DWORD reparse)
    {
        NativeOpenOptions options;
        options.access_mode = 0;
        options.flags = FILE_FLAG_BACKUP_SEMANTICS | reparse;

        auto try_file = NativeFile::open(path, options);
        if (try_file.is_ok())
        {
            return try_file.unwrap().metadata();
        }

        auto error = io::Result<NativeMetadata>::err(std::move(try_file).into_err());

        WIN32_FIND_DATAW wfd = {};
        auto handle = FindFirstFileExW(
            path.c_str(),
            FindExInfoBasic,
            &wfd,
            FindExSearchNameMatch,
            nullptr,
            0);

        if (handle == INVALID_HANDLE_VALUE)
        {
            return error;
        }

        FindClose(handle);
        NativeMetadata metadata(
            wfd.dwFileAttributes,
            wfd.ftCreationTime,
            wfd.ftLastAccessTime,
            wfd.ftLastWriteTime,
            std::optional<FILETIME>{},
            (static_cast<uint64_t>(wfd.nFileSizeHigh) << 32) | static_cast<uint64_t>(wfd.nFileSizeLow),
            (wfd.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) ? wfd.dwReserved0 : 0,
            0,
            0,
            0);

        if (reparse == 0 && metadata.file_type().is_symlink())
        {
            return error;
        }

        return io::Result<NativeMetadata>::ok(std::move(metadata));
    }

    io::Result<NativeMetadata> _try_lstat(const path::PathBuf &path)
    {
        return _metadata(path, FILE_FLAG_OPEN_REPARSE_POINT);
    }

    io::Result<NativeMetadata> _try_stat(const path::PathBuf &path)
    {
        auto result = _metadata(path, 0);
        if (result.is_ok())
        {
            return result;
        }

        auto lstat = _try_lstat(path);
        if (lstat.is_ok() && !lstat.unwrap().file_type().is_symlink())
        {
            return lstat;
        }

        return result;
    }

    io::Result<NativeMetadata> metadata(const path::PathBuf &path)
    {
        return _try_stat(path);
    }
}
