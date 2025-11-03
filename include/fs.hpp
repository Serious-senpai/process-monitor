#pragma once

#include "io.hpp"
#include "path.hpp"
#include "result.hpp"

#ifdef _WIN32
#include "win32/fs.hpp"
#elif defined(__linux__)
#include "linux/fs.hpp"
#endif

namespace fs
{
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
        static io::Result<File> open(const path::PathBuf &path);

        /** @brief Opens a file in write-only mode.
         *
         * @see https://github.com/rust-lang/rust/blob/8182085617878610473f0b88f07fc9803f4b4960/library/std/src/fs.rs#L600-L603
         */
        static io::Result<File> create(const path::PathBuf &path);

        /**
         * @brief Creates a new file in read-write mode; error if the file exists.
         *
         * @see https://github.com/rust-lang/rust/blob/8182085617878610473f0b88f07fc9803f4b4960/library/std/src/fs.rs#L674-L677
         */
        static io::Result<File> create_new(const path::PathBuf &path);

        io::Result<size_t> read(std::span<char> buffer) override;
        io::Result<size_t> write(std::span<const char> buffer) override;
        io::Result<std::monostate> flush() override;
        io::Result<uint64_t> seek(io::SeekFrom position) override;
    };

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
        explicit OpenOptions() noexcept;

        /**
         * @brief Sets the option for read access.
         */
        OpenOptions &read(bool read) noexcept;

        /**
         * @brief Sets the option for write access.
         */
        OpenOptions &write(bool write) noexcept;

        /**
         * @brief Sets the option for the append mode.
         */
        OpenOptions &append(bool append) noexcept;

        /**
         * @brief Sets the option for truncating a previous file.
         */
        OpenOptions &truncate(bool truncate) noexcept;

        /**
         * @brief Sets the option to create a new file, or open it if it already exists.
         */
        OpenOptions &create(bool create) noexcept;

        /**
         * @brief Sets the option to create a new file, failing if it already exists.
         */
        OpenOptions &create_new(bool create_new) noexcept;

        /**
         * @brief Open the file at `path` with the options specified by `this`.
         */
        io::Result<File> open(const path::PathBuf &path) const;
    };

    /**
     * @brief A builder used to create directories in various manners.
     *
     * @see https://doc.rust-lang.org/std/fs/struct.DirBuilder.html
     */
    class DirBuilder : public NonConstructible
    {
    private:
        _fs_impl::NativeDirBuilder _inner;
        bool _recursive;

        io::Result<std::monostate> _create(const path::PathBuf &path) const;
        io::Result<std::monostate> _create_dir_all(const path::PathBuf &path) const;

    public:
        explicit DirBuilder();
        io::Result<std::monostate> create(const path::PathBuf &path) const;
    };

    class Metadata : public NonConstructible
    {
    private:
        _fs_impl::NativeMetadata _inner;

    public:
        explicit Metadata(_fs_impl::NativeMetadata &&inner);
        bool is_dir() const;
        bool is_file() const;
        bool is_symlink() const;
    };

    /**
     * @brief Creates a new, empty directory at the provided path.
     *
     * @see https://doc.rust-lang.org/std/fs/fn.create_dir.html
     */
    io::Result<std::monostate> create_dir(const path::PathBuf &path);
}
