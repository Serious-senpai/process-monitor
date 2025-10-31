#include "fs.hpp"

Result<int, IoError> OpenOptions::get_access_mode() const
{
    int modes = (int(_read) << 2) | (int(_write) << 1) | int(_append);
    switch (modes)
    {
    case 0b100:
        return Result<int, IoError>::ok(O_RDONLY);
    case 0b010:
        return Result<int, IoError>::ok(O_WRONLY);
    case 0b110:
        return Result<int, IoError>::ok(O_RDWR);
    case 0b001:
    case 0b011:
        return Result<int, IoError>::ok(O_WRONLY | O_APPEND);
    case 0b101:
    case 0b111:
        return Result<int, IoError>::ok(O_RDWR | O_APPEND);
    default:
        if (_create || _create_new || _truncate)
        {
            return Result<int, IoError>::err(
                IoError(
                    IoErrorKind::InvalidInput,
                    "creating or truncating a file requires write or append access"));
        }
        else
        {
            return Result<int, IoError>::err(
                IoError(
                    IoErrorKind::InvalidInput,
                    "must specify at least one of read, write, or append access"));
        }
    }
}

Result<int, IoError> OpenOptions::get_creation_mode() const
{
    int modes = (int(_write) << 1) | int(_append);
    switch (modes)
    {
    case 0b10:
        break;
    case 0b00:
        if (_truncate || _create || _create_new)
        {
            return Result<int, IoError>::err(
                IoError(
                    IoErrorKind::InvalidInput,
                    "creating or truncating a file requires write or append access"));
        }

        break;
    default:
        if (_truncate && !_create_new)
        {
            return Result<int, IoError>::err(
                IoError(
                    IoErrorKind::InvalidInput,
                    "creating or truncating a file requires write or append access"));
        }

        break;
    }

    modes = (int(_create) << 2) | (int(_truncate) << 1) | int(_create_new);
    switch (modes)
    {
    case 0b000:
        return Result<int, IoError>::ok(0);
    case 0b100:
        return Result<int, IoError>::ok(O_CREAT);
    case 0b010:
        return Result<int, IoError>::ok(O_TRUNC);
    case 0b110:
        return Result<int, IoError>::ok(O_CREAT | O_TRUNC);
    default:
        return Result<int, IoError>::ok(O_CREAT | O_EXCL);
    }
}
