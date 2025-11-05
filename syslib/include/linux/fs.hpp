#pragma once

#include "io.hpp"
#include "path.hpp"

namespace _fs_impl
{
    /** @brief Documentation for native implementation should not be used for reference. */
    class NativeOpenOptions : public NonConstructible
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

        explicit NativeOpenOptions();

        /** @see https://github.com/rust-lang/rust/blob/8182085617878610473f0b88f07fc9803f4b4960/library/std/src/sys/fs/unix.rs#L1125-L1148 */
        io::Result<int> get_access_mode() const;

        /** @see https://github.com/rust-lang/rust/blob/8182085617878610473f0b88f07fc9803f4b4960/library/std/src/sys/fs/unix.rs#L1150-L1179 */
        io::Result<int> get_creation_mode() const;
    };

    class NativeDirBuilder : public NonConstructible
    {
    private:
        mode_t _mode;

    public:
        explicit NativeDirBuilder();
        io::Result<std::monostate> mkdir(const path::PathBuf &path) const;
        void set_mode(mode_t mode) noexcept;
    };

    class NativeFileType : public NonConstructible
    {
    private:
        mode_t _mode;

    public:
        explicit NativeFileType(mode_t mode);

        bool is_dir() const;
        bool is_file() const;
        bool is_symlink() const;
        bool is(mode_t mode) const;
    };

    class NativeMetadata : public NonConstructible
    {
    private:
        struct stat _stat;

    public:
        explicit NativeMetadata(struct stat stat);

        NativeFileType file_type() const;
        bool is_dir() const;
        bool is_file() const;
        bool is_symlink() const;
    };

    /** @brief Documentation for native implementation should not be used for reference. */
    class NativeFile : public NonConstructible
    {
    private:
        int _fd;

    public:
        explicit NativeFile(int fd);

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
        NativeFile(NativeFile &&other);

        /** @brief Drop guard to close the file descriptor when going out of scope. */
        ~NativeFile();

        /** @see https://github.com/rust-lang/rust/blob/8182085617878610473f0b88f07fc9803f4b4960/library/std/src/sys/fs/unix.rs#L1199-L1214 */
        static io::Result<NativeFile> open(const path::PathBuf &path, const _fs_impl::NativeOpenOptions &options);

        io::Result<size_t> read(std::span<char> buffer);
        io::Result<size_t> write(std::span<const char> buffer);
        io::Result<std::monostate> flush();

        /** @see https://github.com/rust-lang/rust/blob/8182085617878610473f0b88f07fc9803f4b4960/library/std/src/sys/fs/unix.rs#L1572-L1582 */
        io::Result<uint64_t> seek(io::SeekFrom position);
    };

    io::Result<NativeMetadata> metadata(const path::PathBuf &path);
}
