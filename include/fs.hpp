#pragma once

#include "io.hpp"
#include "pch.hpp"
#include "result.hpp"

class OpenOptions
{
private:
    bool _read;
    bool _write;
    bool _append;
    bool _truncate;
    bool _create;
    bool _create_new;

#ifdef _WIN32
#elif defined(__linux__)
    int _flags;
    mode_t _mode;

    /**
     * @see https://github.com/rust-lang/rust/blob/8182085617878610473f0b88f07fc9803f4b4960/library/std/src/sys/fs/unix.rs#L1125-L1148
     */
    Result<int, IoError> get_access_mode() const;

    /**
     * @see https://github.com/rust-lang/rust/blob/8182085617878610473f0b88f07fc9803f4b4960/library/std/src/sys/fs/unix.rs#L1150-L1179
     */
    Result<int, IoError> get_creation_mode() const;
#endif

public:
    /**
     * @see https://github.com/rust-lang/rust/blob/8182085617878610473f0b88f07fc9803f4b4960/library/std/src/sys/fs/unix.rs#L1084-L1097
     */
    explicit OpenOptions();

    OpenOptions &read(bool read);
    OpenOptions &write(bool write);
    OpenOptions &append(bool append);
    OpenOptions &truncate(bool truncate);
    OpenOptions &create(bool create);
    OpenOptions &create_new(bool create_new);
};
