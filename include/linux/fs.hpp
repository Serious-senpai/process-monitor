#pragma once

#include "io.hpp"
#include "pch.hpp"

/** @brief Documentation for native implementation should not be used for reference. */
class _NativeOpenOptions : public NonConstructible
{
public:
    bool read;
    bool write;
    bool append;
    bool truncate;
    bool create;
    bool create_new;

    int flags;
    mode_t mode;

    explicit _NativeOpenOptions();

    /** @see https://github.com/rust-lang/rust/blob/8182085617878610473f0b88f07fc9803f4b4960/library/std/src/sys/fs/unix.rs#L1125-L1148 */
    Result<int, IoError> get_access_mode() const;

    /** @see https://github.com/rust-lang/rust/blob/8182085617878610473f0b88f07fc9803f4b4960/library/std/src/sys/fs/unix.rs#L1150-L1179 */
    Result<int, IoError> get_creation_mode() const;
};

/** @brief Documentation for native implementation should not be used for reference. */
class _NativeFile : public NonConstructible
{
private:
    int _fd;

public:
    explicit _NativeFile(int fd);

    /**
     * @brief Obtain ownership of another file descriptor.
     *
     * Also, we must define a move constructor manually because having a user-defined destructor
     * inhibits the implicit declaration of a move constructor. Refer
     * [here](https://en.cppreference.com/w/cpp/language/move_constructor.html) for details.
     *
     * Damn these standard shits. A little default here, some other implicit stuff there, how tf
     * am I supposed to remember all of them?
     */
    _NativeFile(_NativeFile &&other);

    /** @brief Drop guard to close the file descriptor when going out of scope. */
    ~_NativeFile();

    /** @see https://github.com/rust-lang/rust/blob/8182085617878610473f0b88f07fc9803f4b4960/library/std/src/sys/fs/unix.rs#L1199-L1214 */
    static Result<_NativeFile, IoError> open(const char *path, const _NativeOpenOptions &options);

    Result<size_t, IoError> read(std::span<char> buffer);
    Result<size_t, IoError> write(std::span<const char> buffer);
    Result<std::monostate, IoError> flush();

    /** @see https://github.com/rust-lang/rust/blob/8182085617878610473f0b88f07fc9803f4b4960/library/std/src/sys/fs/unix.rs#L1572-L1582 */
    Result<uint64_t, IoError> seek(SeekFrom position);
};
