#pragma once

#include "io.hpp"
#include "pch.hpp"
#include "result.hpp"

#ifdef _WIN32
#elif defined(__linux__)
#include "linux/fs.hpp"
#endif

/**
 * @brief Options and flags which can be used to configure how a file is opened.
 *
 * @see https://doc.rust-lang.org/std/fs/struct.OpenOptions.html
 */
class OpenOptions
{
private:
    _OpenOptions _inner;

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

    /**
     * @brief Open the file at `path` with the options specified by `this`.
     */
    Result<class File, IoError> open(const char *path);
};

/**
 * @brief An object providing access to an open file on the filesystem.
 *
 * @see https://doc.rust-lang.org/std/fs/struct.File.html
 */
class File : public NonConstructible, public Read, public Write, public Seek
{
private:
    _File _inner;

public:
    explicit File(_File &&inner);

    /** @brief Attempts to open a file in read-only mode. */
    static Result<File, IoError> open(const char *path);

    /** @brief Opens a file in write-only mode. */
    static Result<File, IoError> create(const char *path);

    Result<size_t, IoError> read(std::span<char> buffer) override;
    Result<size_t, IoError> write(std::span<const char> buffer) override;
    Result<std::monostate, IoError> flush() override;
    Result<u_int64_t, IoError> seek(SeekFrom position) override;
};
