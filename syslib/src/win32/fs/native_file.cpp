#include "fs.hpp"
#include "win32/path.hpp"
#include "win32/pch.hpp"

namespace _fs_impl
{
    io::Result<size_t> NativeFile::_synchronous_read(std::span<char> buffer, std::optional<uint64_t> offset)
    {
        IO_STATUS_BLOCK io_status = {};
        io_status.Status = STATUS_PENDING;
        io_status.Information = 0;

        auto bytes_to_read = static_cast<ULONG>(std::min<size_t>(buffer.size(), MAXULONG32));

        LARGE_INTEGER bytes_offset = {};
        bytes_offset.QuadPart = offset.has_value() ? static_cast<LONGLONG>(*offset) : -1;

        auto status = NtReadFile(
            _handle,
            nullptr,
            nullptr,
            nullptr,
            &io_status,
            buffer.data(),
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
            return io::Result<size_t>::ok(0);
        default:
            if (NT_SUCCESS(status))
            {
                return io::Result<size_t>::ok(static_cast<size_t>(io_status.Information));
            }
            else
            {
                auto error = RtlNtStatusToDosError(status);
                return io::Result<size_t>::err(io::IoError(io::IoErrorKind::Os, std::format("NtReadFile: OS error {}", error)));
            }
        }
    }

    io::Result<size_t> NativeFile::_synchronous_write(std::span<const char> buffer, std::optional<uint64_t> offset)
    {
        IO_STATUS_BLOCK io_status = {};
        io_status.Status = STATUS_PENDING;
        io_status.Information = 0;

        auto bytes_to_read = static_cast<ULONG>(std::min<size_t>(buffer.size(), MAXULONG32));

        LARGE_INTEGER bytes_offset = {};
        bytes_offset.QuadPart = offset.has_value() ? static_cast<LONGLONG>(*offset) : -1;

        auto status = NtWriteFile(
            _handle,
            nullptr,
            nullptr,
            nullptr,
            &io_status,
            const_cast<char *>(buffer.data()),
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
            return io::Result<size_t>::ok(0);
        default:
            if (NT_SUCCESS(status))
            {
                return io::Result<size_t>::ok(static_cast<size_t>(io_status.Information));
            }
            else
            {
                auto error = RtlNtStatusToDosError(status);
                return io::Result<size_t>::err(io::IoError(io::IoErrorKind::Os, std::format("NtWriteFile: OS error {}", error)));
            }
        }
    }

    NativeFile::NativeFile(HANDLE handle) : CloseHandleGuard(handle) {}

    io::Result<NativeFile> NativeFile::open(const path::PathBuf &path, const NativeOpenOptions &options)
    {
        auto verbatim = get_long_path(path::PathBuf(path), true);
        auto creation = SHORT_CIRCUIT(NativeFile, options.get_creation_mode());

        SECURITY_ATTRIBUTES sa = {};
        sa.nLength = sizeof(SECURITY_ATTRIBUTES);
        sa.lpSecurityDescriptor = nullptr;
        sa.bInheritHandle = options.inherit_handle;

        auto handle = CreateFileW(
            verbatim.is_ok() ? verbatim.unwrap().c_str() : path.c_str(),
            SHORT_CIRCUIT(NativeFile, options.get_access_mode()),
            options.share_mode,
            options.inherit_handle ? &sa : nullptr,
            creation,
            options.get_flags_and_attributes(),
            nullptr);

        if (handle == INVALID_HANDLE_VALUE)
        {
            return io::Result<NativeFile>::err(io::IoError(io::IoErrorKind::Os, std::format("CreateFileW: OS error {}", GetLastError())));
        }

        CloseHandleGuard guard(handle);

        // See https://github.com/rust-lang/rust/issues/115745
        // Fixed by https://github.com/rust-lang/rust/pull/116438/files#diff-e8df55f38a9a224cf1cfd40e6c535535aa66e8073cc8d9b959308659ba1de1f9
        if (options.truncate && creation == OPEN_ALWAYS && GetLastError())
        {
            FILE_ALLOCATION_INFO alloc = {};
            alloc.AllocationSize = LARGE_INTEGER{0};

            OS_CVT(NativeFile, SetFileInformationByHandle(handle, FileAllocationInfo, &alloc, sizeof(FILE_ALLOCATION_INFO)));
        }

        return io::Result<NativeFile>::ok(NativeFile(std::move(guard).into_handle()));
    }

    io::Result<size_t> NativeFile::read(std::span<char> buffer)
    {
        return _synchronous_read(buffer, std::nullopt);
    }

    io::Result<size_t> NativeFile::write(std::span<const char> buffer)
    {
        return _synchronous_write(buffer, std::nullopt);
    }

    io::Result<std::monostate> NativeFile::flush()
    {
        return io::Result<std::monostate>::ok(std::monostate{});
    }

    io::Result<uint64_t> NativeFile::seek(io::SeekFrom position)
    {
        int whence;
        switch (position.type)
        {
        case io::SeekFrom::Start:
            whence = FILE_BEGIN;
            break;
        case io::SeekFrom::Current:
            whence = FILE_CURRENT;
            break;
        case io::SeekFrom::End:
            whence = FILE_END;
            break;
        default:
            return io::Result<uint64_t>::err(io::IoError(io::IoErrorKind::InvalidInput, "invalid seek type"));
        }

        LARGE_INTEGER pos = {};
        pos.QuadPart = position.offset;

        LARGE_INTEGER new_pos = {};
        OS_CVT(uint64_t, SetFilePointerEx(_handle, pos, reinterpret_cast<PLARGE_INTEGER>(&new_pos), whence));

        return io::Result<uint64_t>::ok(static_cast<uint64_t>(new_pos.QuadPart));
    }

    io::Result<NativeMetadata> NativeFile::metadata()
    {
        BY_HANDLE_FILE_INFORMATION info = {};
        OS_CVT(NativeMetadata, GetFileInformationByHandle(_handle, &info));

        DWORD reparse = 0;
        if (info.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT)
        {
            FILE_ATTRIBUTE_TAG_INFO tag_info = {};
            OS_CVT(NativeMetadata, GetFileInformationByHandleEx(_handle, FileAttributeTagInfo, &tag_info, sizeof(FILE_ATTRIBUTE_TAG_INFO)));

            if (tag_info.FileAttributes & FILE_ATTRIBUTE_REPARSE_POINT)
            {
                reparse = tag_info.ReparseTag;
            }
        }

        return io::Result<NativeMetadata>::ok(
            NativeMetadata(
                info.dwFileAttributes,
                info.ftCreationTime,
                info.ftLastAccessTime,
                info.ftLastWriteTime,
                std::nullopt,
                (static_cast<uint64_t>(info.nFileSizeHigh) << 32) | static_cast<uint64_t>(info.nFileSizeLow),
                reparse,
                info.dwVolumeSerialNumber,
                info.nNumberOfLinks,
                (static_cast<uint64_t>(info.nFileIndexHigh) << 32) | static_cast<uint64_t>(info.nFileIndexLow)));
    }
}
