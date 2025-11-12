#pragma once

#include "io.hpp"
#include "win32/handle.hpp"

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

        std::optional<DWORD> access_mode;
        DWORD share_mode;
        DWORD flags;
        DWORD attributes;
        DWORD security_qos_flags;
        BOOL inherit_handle;

        explicit NativeOpenOptions();

        /** @see https://github.com/rust-lang/rust/blob/8182085617878610473f0b88f07fc9803f4b4960/library/std/src/sys/fs/windows.rs#L250-L276 */
        io::Result<DWORD> get_access_mode() const;

        /** @see https://github.com/rust-lang/rust/blob/8182085617878610473f0b88f07fc9803f4b4960/library/std/src/sys/fs/windows.rs#L278-L308 */
        io::Result<DWORD> get_creation_mode() const;

        /** @see https://github.com/rust-lang/rust/blob/8182085617878610473f0b88f07fc9803f4b4960/library/std/src/sys/fs/windows.rs#L310-L315 */
        DWORD get_flags_and_attributes() const;
    };

    class NativeDirBuilder : public NonConstructible
    {
    public:
        explicit NativeDirBuilder();
        io::Result<std::monostate> mkdir(const path::PathBuf &path) const;
    };

    class NativeFileType : public NonConstructible
    {
    private:
        bool _is_dir;
        bool _is_symlink;

    public:
        explicit NativeFileType(DWORD attributes, DWORD reparse_tag);

        bool is_dir() const;
        bool is_file() const;
        bool is_symlink() const;
        bool is_symlink_dir() const;
        bool is_symlink_file() const;
    };

    class NativeMetadata : public NonConstructible
    {
    public:
        DWORD attributes;
        FILETIME creation_time;
        FILETIME last_access_time;
        FILETIME last_write_time;
        std::optional<FILETIME> change_time;
        uint64_t file_size;
        DWORD reparse_tag;
        DWORD volume_serial_number;
        DWORD number_of_links;
        uint64_t file_index;

        explicit NativeMetadata(
            DWORD attributes,
            FILETIME creation_time,
            FILETIME last_access_time,
            FILETIME last_write_time,
            std::optional<FILETIME> change_time,
            uint64_t file_size,
            DWORD reparse_tag,
            DWORD volume_serial_number,
            DWORD number_of_links,
            uint64_t file_index);

        NativeFileType file_type() const;
        bool is_dir() const;
        bool is_file() const;
        bool is_symlink() const;
    };

    /** @brief Documentation for native implementation should not be used for reference. */
    class NativeFile : public CloseHandleGuard
    {
    private:
        /** @see https://github.com/rust-lang/rust/blob/8182085617878610473f0b88f07fc9803f4b4960/library/std/src/sys/pal/windows/handle.rs#L245-L300 */
        io::Result<size_t> _synchronous_read(std::span<char> buffer, std::optional<uint64_t> offset);

        /** @see https://github.com/rust-lang/rust/blob/8182085617878610473f0b88f07fc9803f4b4960/library/std/src/sys/pal/windows/handle.rs#L302-L346 */
        io::Result<size_t> _synchronous_write(std::span<const char> buffer, std::optional<uint64_t> offset);

    public:
        explicit NativeFile(HANDLE handle);

        /** @see https://github.com/rust-lang/rust/blob/8182085617878610473f0b88f07fc9803f4b4960/library/std/src/sys/fs/windows.rs#L319-L367 */
        static io::Result<NativeFile> open(const path::PathBuf &path, const NativeOpenOptions &options);

        /** @see https://github.com/rust-lang/rust/blob/8182085617878610473f0b88f07fc9803f4b4960/library/std/src/sys/pal/windows/handle.rs#L80-L94 */
        io::Result<size_t> read(std::span<char> buffer);

        /** @see https://github.com/rust-lang/rust/blob/8182085617878610473f0b88f07fc9803f4b4960/library/std/src/sys/pal/windows/handle.rs#L220-L222 */
        io::Result<size_t> write(std::span<const char> buffer);

        /** @see https://github.com/rust-lang/rust/blob/8182085617878610473f0b88f07fc9803f4b4960/library/std/src/sys/fs/windows.rs#L629-L631 */
        io::Result<std::monostate> flush();

        /** @see https://github.com/rust-lang/rust/blob/8182085617878610473f0b88f07fc9803f4b4960/library/std/src/sys/fs/windows.rs#L633-L645 */
        io::Result<uint64_t> seek(io::SeekFrom position);

        io::Result<NativeMetadata> metadata();
    };

    io::Result<NativeMetadata> metadata(const path::PathBuf &path);

    class NativeDirEntry : public NonConstructible
    {
    private:
        HANDLE _dir;
        std::optional<WIN32_FIND_DATAW> _entry;

    public:
        explicit NativeDirEntry(HANDLE dir, std::optional<WIN32_FIND_DATAW> entry);
        NativeDirEntry(NativeDirEntry &&other);
        ~NativeDirEntry();

        io::Result<bool> next();
    };

    class NativeReadDir : public NonConstructible
    {
    private:
        path::PathBuf _path;

    public:
        explicit NativeReadDir(path::PathBuf &&path);

        const path::PathBuf &path() const noexcept;
        io::Result<NativeDirEntry> begin() const;
    };
}
