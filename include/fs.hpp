#pragma once

#include "io.hpp"
#include "pch.hpp"
#include "result.hpp"

#ifdef _WIN32
#include "win32/fs.hpp"
#elif defined(__linux__)
#include "linux/fs.hpp"
#endif

namespace fs
{
    /**
     * @brief Options and flags which can be used to configure how a file is opened.
     *
     * @see https://doc.rust-lang.org/std/fs/struct.OpenOptions.html
     */
    class OpenOptions
    {
    private:
        _fs_impl::NativeOpenOptions _inner;

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
        io::Result<class File> open(const char *path);
    };

    /**
     * @brief An object providing access to an open file on the filesystem.
     *
     * @see https://doc.rust-lang.org/std/fs/struct.File.html
     */
    class File : public NonConstructible, public io::Read, public io::Write, public io::Seek
    {
    private:
        _fs_impl::NativeFile _inner;

    public:
        explicit File(_fs_impl::NativeFile &&inner);

        /**
         * @brief Attempts to open a file in read-only mode.
         *
         * @see https://github.com/rust-lang/rust/blob/8182085617878610473f0b88f07fc9803f4b4960/library/std/src/fs.rs#L532-L535
         */
        static io::Result<File> open(const char *path);

        /** @brief Opens a file in write-only mode.
         *
         * @see https://github.com/rust-lang/rust/blob/8182085617878610473f0b88f07fc9803f4b4960/library/std/src/fs.rs#L600-L603
         */
        static io::Result<File> create(const char *path);

        /**
         * @brief Creates a new file in read-write mode; error if the file exists.
         *
         * @see https://github.com/rust-lang/rust/blob/8182085617878610473f0b88f07fc9803f4b4960/library/std/src/fs.rs#L674-L677
         */
        static io::Result<File> create_new(const char *path);

        io::Result<size_t> read(std::span<char> buffer) override;
        io::Result<size_t> write(std::span<const char> buffer) override;
        io::Result<std::monostate> flush() override;
        io::Result<uint64_t> seek(io::SeekFrom position) override;
    };
}
