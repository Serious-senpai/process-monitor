#include "fs.hpp"

_NativeOpenOptions::_NativeOpenOptions()
    : NonConstructible(NonConstructibleTag::TAG),
      read(false),
      write(false),
      append(false),
      truncate(false),
      create(false),
      create_new(false),
      access_mode(std::nullopt),
      share_mode(FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE),
      flags(0),
      attributes(0),
      security_qos_flags(0),
      inherit_handle(FALSE) {}

Result<DWORD, IoError> _NativeOpenOptions::get_access_mode() const
{
    if (access_mode.has_value())
    {
        return Result<DWORD, IoError>::ok(static_cast<int>(access_mode.value()));
    }

    int modes = (static_cast<int>(read) << 2) |
                (static_cast<int>(write) << 1) |
                (static_cast<int>(append) << 0);
    switch (modes)
    {
    case 0b100:
        return Result<DWORD, IoError>::ok(GENERIC_READ);
    case 0b010:
        return Result<DWORD, IoError>::ok(GENERIC_WRITE);
    case 0b110:
        return Result<DWORD, IoError>::ok(GENERIC_READ | GENERIC_WRITE);
    case 0b001:
    case 0b011:
        return Result<DWORD, IoError>::ok(FILE_GENERIC_WRITE & ~FILE_WRITE_DATA);
    case 0b101:
    case 0b111:
        return Result<DWORD, IoError>::ok(GENERIC_READ | (FILE_GENERIC_WRITE & ~FILE_WRITE_DATA));
    default:
        if (create || create_new || truncate)
        {
            return Result<DWORD, IoError>::err(
                IoError(
                    IoErrorKind::InvalidInput,
                    "creating or truncating a file requires write or append access"));
        }
        else
        {
            return Result<DWORD, IoError>::err(
                IoError(
                    IoErrorKind::InvalidInput,
                    "must specify at least one of read, write, or append access"));
        }
    }
}

Result<DWORD, IoError> _NativeOpenOptions::get_creation_mode() const
{
    int modes = (static_cast<int>(write) << 1) | (static_cast<int>(append) << 0);
    switch (modes)
    {
    case 0b10:
        break;
    case 0b00:
        if (truncate || create || create_new)
        {
            return Result<DWORD, IoError>::err(
                IoError(
                    IoErrorKind::InvalidInput,
                    "creating or truncating a file requires write or append access"));
        }

        break;
    default:
        if (truncate && !create_new)
        {
            return Result<DWORD, IoError>::err(
                IoError(
                    IoErrorKind::InvalidInput,
                    "creating or truncating a file requires write or append access"));
        }

        break;
    }

    modes = (static_cast<int>(create) << 2) | (static_cast<int>(truncate) << 1) | (static_cast<int>(create_new) << 0);
    switch (modes)
    {
    case 0b000:
        return Result<DWORD, IoError>::ok(OPEN_EXISTING);
    case 0b100:
        return Result<DWORD, IoError>::ok(OPEN_ALWAYS);
    case 0b010:
        return Result<DWORD, IoError>::ok(TRUNCATE_EXISTING);
    case 0b110:
        // See https://github.com/rust-lang/rust/issues/115745
        // Fixed by https://github.com/rust-lang/rust/pull/116438/files#diff-e8df55f38a9a224cf1cfd40e6c535535aa66e8073cc8d9b959308659ba1de1f9
        return Result<DWORD, IoError>::ok(OPEN_ALWAYS);
    default:
        return Result<DWORD, IoError>::ok(CREATE_NEW);
    }
}

DWORD _NativeOpenOptions::get_flags_and_attributes() const
{
    return flags | attributes | security_qos_flags | (create_new ? FILE_FLAG_OPEN_REPARSE_POINT : 0);
}
