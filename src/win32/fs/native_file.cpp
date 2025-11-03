#include "fs.hpp"
#include "win32/path.hpp"
#include "win32/pch.hpp"

Result<size_t, IoError> _NativeFile::_synchronous_read(char *buffer, size_t len, std::optional<uint64_t> offset)
{
    IO_STATUS_BLOCK io_status = {};
    io_status.Status = STATUS_PENDING;
    io_status.Information = 0;

    auto bytes_to_read = static_cast<ULONG>(std::min<size_t>(len, MAXULONG32));

    LARGE_INTEGER bytes_offset = {};
    bytes_offset.QuadPart = offset.has_value() ? static_cast<LONGLONG>(*offset) : -1;

    auto status = NtReadFile(
        _handle,
        nullptr,
        nullptr,
        nullptr,
        &io_status,
        buffer,
        bytes_to_read,
        offset.has_value() ? &bytes_offset : nullptr,
        nullptr);

    if (status == STATUS_PENDING)
    {
        WaitForSingleObject(_handle, INFINITE);
        status = io_status.Status;
    }

    switch (status)
    {
    case STATUS_PENDING:
        throw std::runtime_error("I/O error: operation failed to complete synchronously");
    case STATUS_END_OF_FILE:
        return Result<size_t, IoError>::ok(0);
    default:
        if (NT_SUCCESS(status))
        {
            return Result<size_t, IoError>::ok(static_cast<size_t>(io_status.Information));
        }
        else
        {
            auto error = RtlNtStatusToDosError(status);
            return Result<size_t, IoError>::err(IoError(IoErrorKind::Os, std::format("NtReadFile: OS error {}", error)));
        }
    }
}

Result<size_t, IoError> _NativeFile::_synchronous_write(const char *buffer, size_t len, std::optional<uint64_t> offset)
{
    IO_STATUS_BLOCK io_status = {};
    io_status.Status = STATUS_PENDING;
    io_status.Information = 0;

    auto bytes_to_read = static_cast<ULONG>(std::min<size_t>(len, MAXULONG32));

    LARGE_INTEGER bytes_offset = {};
    bytes_offset.QuadPart = offset.has_value() ? static_cast<LONGLONG>(*offset) : -1;

    auto status = NtWriteFile(
        _handle,
        nullptr,
        nullptr,
        nullptr,
        &io_status,
        const_cast<char *>(buffer),
        bytes_to_read,
        offset.has_value() ? &bytes_offset : nullptr,
        nullptr);

    if (status == STATUS_PENDING)
    {
        WaitForSingleObject(_handle, INFINITE);
        status = io_status.Status;
    }

    switch (status)
    {
    case STATUS_PENDING:
        throw std::runtime_error("I/O error: operation failed to complete synchronously");
    case STATUS_END_OF_FILE:
        return Result<size_t, IoError>::ok(0);
    default:
        if (NT_SUCCESS(status))
        {
            return Result<size_t, IoError>::ok(static_cast<size_t>(io_status.Information));
        }
        else
        {
            auto error = RtlNtStatusToDosError(status);
            return Result<size_t, IoError>::err(IoError(IoErrorKind::Os, std::format("NtWriteFile: OS error {}", error)));
        }
    }
}

_NativeFile::_NativeFile(HANDLE handle) : CloseHandleGuard(handle) {}

Result<_NativeFile, IoError> _NativeFile::open(const char *path, const _NativeOpenOptions &options)
{
    auto wpath = SHORT_CIRCUIT(_NativeFile, to_widestring(path));
    auto creation = SHORT_CIRCUIT(_NativeFile, options.get_creation_mode());

    SECURITY_ATTRIBUTES sa = {};
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
        return Result<_NativeFile, IoError>::err(IoError(IoErrorKind::Os, std::format("CreateFileW: OS error {}", GetLastError())));
    }

    CloseHandleGuard guard(handle);

    // See https://github.com/rust-lang/rust/issues/115745
    // Fixed by https://github.com/rust-lang/rust/pull/116438/files#diff-e8df55f38a9a224cf1cfd40e6c535535aa66e8073cc8d9b959308659ba1de1f9
    if (options.truncate && creation == OPEN_ALWAYS && GetLastError())
    {
        FILE_ALLOCATION_INFO alloc = {};
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
    return _synchronous_read(buffer.data(), buffer.size(), std::nullopt);
}

Result<size_t, IoError> _NativeFile::write(std::span<const char> buffer)
{
    return _synchronous_write(buffer.data(), buffer.size(), std::nullopt);
}

Result<std::monostate, IoError> _NativeFile::flush()
{
    return Result<std::monostate, IoError>::ok(std::monostate{});
}

Result<uint64_t, IoError> _NativeFile::seek(SeekFrom position)
{
    int whence;
    switch (position.type)
    {
    case SeekFrom::Start:
        whence = FILE_BEGIN;
        break;
    case SeekFrom::Current:
        whence = FILE_CURRENT;
        break;
    case SeekFrom::End:
        whence = FILE_END;
        break;
    default:
        return Result<uint64_t, IoError>::err(IoError(IoErrorKind::InvalidInput, "invalid seek type"));
    }

    LARGE_INTEGER pos = {};
    pos.QuadPart = position.offset;

    LARGE_INTEGER new_pos = {};
    SetFilePointerEx(_handle, pos, reinterpret_cast<PLARGE_INTEGER>(&new_pos), whence);

    return Result<uint64_t, IoError>::ok(static_cast<uint64_t>(new_pos.QuadPart));
}
