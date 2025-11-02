#include "fs.hpp"
#include "win32/path.hpp"

_NativeFile::_NativeFile(HANDLE handle) : CloseHandleGuard(handle) {}

Result<_NativeFile, IoError> _NativeFile::open(const char *path, const _NativeOpenOptions &options)
{
    auto wpath = SHORT_CIRCUIT(_NativeFile, to_widestring(path));
    auto creation = SHORT_CIRCUIT(_NativeFile, options.get_creation_mode());

    SECURITY_ATTRIBUTES sa;
    sa.nLength = sizeof(SECURITY_ATTRIBUTES);
    sa.lpSecurityDescriptor = nullptr;
    sa.bInheritHandle = options.inherit_handle;

    auto handle = CreateFileW(
        wpath.c_str(),
        SHORT_CIRCUIT(_NativeFile, options.get_access_mode()),
        options.share_mode,
        options.inherit_handle ? &sa : nullptr,
        creation,
        options.get_flags_and_attributes(),
        nullptr);

    if (handle == INVALID_HANDLE_VALUE)
    {
        return Result<_NativeFile, IoError>::err(
            IoError(IoErrorKind::Os, std::format("CreateFileW: OS error {}", GetLastError())));
    }

    CloseHandleGuard guard(handle);

    // See https://github.com/rust-lang/rust/issues/115745
    // Fixed by https://github.com/rust-lang/rust/pull/116438/files#diff-e8df55f38a9a224cf1cfd40e6c535535aa66e8073cc8d9b959308659ba1de1f9
    if (options.truncate && creation == OPEN_ALWAYS && GetLastError())
    {
        FILE_ALLOCATION_INFO alloc;
        alloc.AllocationSize = LARGE_INTEGER{0};

        if (SetFileInformationByHandle(handle, FileAllocationInfo, &alloc, sizeof(FILE_ALLOCATION_INFO)) == 0)
        {
            return Result<_NativeFile, IoError>::err(
                IoError(IoErrorKind::Os, std::format("SetFileInformationByHandle: OS error {}", GetLastError())));
        }
    }

    return Result<_NativeFile, IoError>::ok(_NativeFile(std::move(guard).into_handle()));
}

Result<size_t, IoError> _NativeFile::read(std::span<char> buffer)
{
    if (buffer.empty())
    {
        return Result<size_t, IoError>::ok(0);
    }

    // TODO
}

Result<size_t, IoError> _NativeFile::write(std::span<const char> buffer)
{
    if (buffer.empty())
    {
        return Result<size_t, IoError>::ok(0);
    }

    // TODO
}

Result<std::monostate, IoError> _NativeFile::flush()
{
    // TODO
}

Result<uint64_t, IoError> _NativeFile::seek(SeekFrom position)
{
    // TODO
}
