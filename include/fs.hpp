#pragma once

#include "io.hpp"
#include "pch.hpp"
#include "result.hpp"

/**
 * @brief Options and flags which can be used to configure how a file is opened.
 *
 * @see https://doc.rust-lang.org/std/fs/struct.OpenOptions.html
 */
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

    Result<int, IoError> _get_access_mode() const;
    Result<int, IoError> _get_creation_mode() const;

#endif

public:
    /**
     * @brief Creates a blank new set of options ready for configuration.
     *
     * All options are initially set to `false`.
     *
     * @see https://github.com/rust-lang/rust/blob/8182085617878610473f0b88f07fc9803f4b4960/library/std/src/sys/fs/unix.rs#L1084-L1097
     */
    explicit OpenOptions();

    /**
     * @brief Sets the option for read access.
     */
    OpenOptions &read(bool read);

    /**
     * @brief Sets the option for write access.
     */
    OpenOptions &write(bool write);

    /**
     * @brief Sets the option for the append mode.
     */
    OpenOptions &append(bool append);

    /**
     * @brief Sets the option for truncating a previous file.
     */
    OpenOptions &truncate(bool truncate);

    /**
     * @brief Sets the option to create a new file, or open it if it already exists.
     */
    OpenOptions &create(bool create);

    /**
     * @brief Sets the option to create a new file, failing if it already exists.
     */
    OpenOptions &create_new(bool create_new);
};

/**
 * @brief An object providing access to an open file on the filesystem.
 *
 * @see https://doc.rust-lang.org/std/fs/struct.File.html
 */
class File : public NonConstructible, public Read, public Write, public Seek
{
private:
    int _fd;

public:
    Result<size_t, IoError> read(std::span<char> buffer) override;
    Result<size_t, IoError> write(std::span<const char> buffer) override;
    Result<std::monostate, IoError> flush() override;
    Result<u_int64_t, IoError> seek(SeekFrom position) override;
};
