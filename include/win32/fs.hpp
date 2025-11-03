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

    /** @brief Documentation for native implementation should not be used for reference. */
    class NativeFile : public CloseHandleGuard
    {
    private:
        /** @see https://github.com/rust-lang/rust/blob/8182085617878610473f0b88f07fc9803f4b4960/library/std/src/sys/pal/windows/handle.rs#L245-L300 */
        io::Result<size_t> _synchronous_read(char *buffer, size_t len, std::optional<uint64_t> offset);

        /** @see https://github.com/rust-lang/rust/blob/8182085617878610473f0b88f07fc9803f4b4960/library/std/src/sys/pal/windows/handle.rs#L302-L346 */
        io::Result<size_t> _synchronous_write(const char *buffer, size_t len, std::optional<uint64_t> offset);

    public:
        explicit NativeFile(HANDLE handle);

        /** @see https://github.com/rust-lang/rust/blob/8182085617878610473f0b88f07fc9803f4b4960/library/std/src/sys/fs/windows.rs#L319-L367 */
        static io::Result<NativeFile> open(const char *path, const NativeOpenOptions &options);

        /** @see https://github.com/rust-lang/rust/blob/8182085617878610473f0b88f07fc9803f4b4960/library/std/src/sys/pal/windows/handle.rs#L80-L94 */
        io::Result<size_t> read(std::span<char> buffer);

        /** @see https://github.com/rust-lang/rust/blob/8182085617878610473f0b88f07fc9803f4b4960/library/std/src/sys/pal/windows/handle.rs#L220-L222 */
        io::Result<size_t> write(std::span<const char> buffer);

        /** @see https://github.com/rust-lang/rust/blob/8182085617878610473f0b88f07fc9803f4b4960/library/std/src/sys/fs/windows.rs#L629-L631 */
        io::Result<std::monostate> flush();

        /** @see https://github.com/rust-lang/rust/blob/8182085617878610473f0b88f07fc9803f4b4960/library/std/src/sys/fs/windows.rs#L633-L645 */
        io::Result<uint64_t> seek(io::SeekFrom position);
    };
}
