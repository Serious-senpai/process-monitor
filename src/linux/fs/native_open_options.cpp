#include "fs.hpp"

namespace _fs_impl
{
    NativeOpenOptions::NativeOpenOptions()
        : NonConstructible(NonConstructibleTag::TAG),
          read(false),
          write(false),
          append(false),
          truncate(false),
          create(false),
          create_new(false),
          flags(0),
          mode(0644) {}

    io::Result<int> NativeOpenOptions::get_access_mode() const
    {
        int modes = (static_cast<int>(read) << 2) |
                    (static_cast<int>(write) << 1) |
                    (static_cast<int>(append) << 0);
        switch (modes)
        {
        case 0b100:
            return io::Result<int>::ok(O_RDONLY);
        case 0b010:
            return io::Result<int>::ok(O_WRONLY);
        case 0b110:
            return io::Result<int>::ok(O_RDWR);
        case 0b001:
        case 0b011:
            return io::Result<int>::ok(O_WRONLY | O_APPEND);
        case 0b101:
        case 0b111:
            return io::Result<int>::ok(O_RDWR | O_APPEND);
        default:
            if (create || create_new || truncate)
            {
                return io::Result<int>::err(
                    io::IoError(
                        io::IoErrorKind::InvalidInput,
                        "creating or truncating a file requires write or append access"));
            }
            else
            {
                return io::Result<int>::err(
                    io::IoError(
                        io::IoErrorKind::InvalidInput,
                        "must specify at least one of read, write, or append access"));
            }
        }
    }

    io::Result<int> NativeOpenOptions::get_creation_mode() const
    {
        int modes = (static_cast<int>(write) << 1) | (static_cast<int>(append) << 0);
        switch (modes)
        {
        case 0b10:
            break;
        case 0b00:
            if (truncate || create || create_new)
            {
                return io::Result<int>::err(
                    io::IoError(
                        io::IoErrorKind::InvalidInput,
                        "creating or truncating a file requires write or append access"));
            }

            break;
        default:
            if (truncate && !create_new)
            {
                return io::Result<int>::err(
                    io::IoError(
                        io::IoErrorKind::InvalidInput,
                        "creating or truncating a file requires write or append access"));
            }

            break;
        }

        modes = (static_cast<int>(create) << 2) | (static_cast<int>(truncate) << 1) | (static_cast<int>(create_new) << 0);
        switch (modes)
        {
        case 0b000:
            return io::Result<int>::ok(0);
        case 0b100:
            return io::Result<int>::ok(O_CREAT);
        case 0b010:
            return io::Result<int>::ok(O_TRUNC);
        case 0b110:
            return io::Result<int>::ok(O_CREAT | O_TRUNC);
        default:
            return io::Result<int>::ok(O_CREAT | O_EXCL);
        }
    }
}
