#pragma once

#include "error.hpp"
#include "pch.hpp"
#include "result.hpp"

namespace io
{
    /**
     * @brief A list specifying general categories of I/O error.
     *
     * @see https://doc.rust-lang.org/std/io/enum.ErrorKind.html
     */
    enum ErrorKind
    {
        /** @brief An entity was not found, often a file. */
        NotFound,
        /** @brief The operation lacked the necessary privileges to complete. */
        PermissionDenied,
        /** @brief The connection was refused by the remote server. */
        ConnectionRefused,
        /** @brief The connection was reset by the remote server. */
        ConnectionReset,
        /** @brief The remote host is not reachable. */
        HostUnreachable,
        /** @brief The network containing the remote host is not reachable. */
        NetworkUnreachable,
        /** @brief The connection was aborted (terminated) by the remote server. */
        ConnectionAborted,
        /** @brief The network operation failed because it was not connected yet. */
        NotConnected,
        /** @brief A socket address could not be bound because the address is already in use elsewhere. */
        AddrInUse,
        /** @brief A nonexistent interface was requested or the requested address was not local. */
        AddrNotAvailable,
        /** @brief The system's networking is down. */
        NetworkDown,
        /** @brief The operation failed because a pipe was closed. */
        BrokenPipe,
        /** @brief An entity already exists, often a file. */
        AlreadyExists,
        /** @brief The operation needs to block to complete, but the blocking operation was requested to not occur. */
        WouldBlock,
        /** @brief A filesystem object is, unexpectedly, not a directory. */
        NotADirectory,
        /** @brief The filesystem object is, unexpectedly, a directory. */
        IsADirectory,
        /** @brief A non-empty directory was specified where an empty directory was expected. */
        DirectoryNotEmpty,
        /** @brief The filesystem or storage medium is read-only, but a write operation was attempted. */
        ReadOnlyFilesystem,
        /** @brief Loop in the filesystem or IO subsystem; often, too many levels of symbolic links. */
        FilesystemLoop,
        /** @brief Stale network file handle. */
        StaleNetworkFileHandle,
        /** @brief A parameter was incorrect. */
        InvalidInput,
        /** @brief Data not valid for the operation were encountered. */
        InvalidData,
        /** @brief The I/O operation's timeout expired, causing it to be canceled. */
        TimedOut,
        /** @brief An error returned when an operation could not be completed because a call to write returned Ok(0). */
        WriteZero,
        /** @brief The underlying storage (typically, a filesystem) is full. */
        StorageFull,
        /** @brief Seek on unseekable file. */
        NotSeekable,
        /** @brief Filesystem quota or some other kind of quota was exceeded. */
        QuotaExceeded,
        /** @brief File larger than allowed or supported. */
        FileTooLarge,
        /** @brief Resource is busy. */
        ResourceBusy,
        /** @brief Executable file is busy. */
        ExecutableFileBusy,
        /** @brief Deadlock (avoided). */
        Deadlock,
        /** @brief Cross-device or cross-filesystem (hard) link or rename. */
        CrossesDevices,
        /** @brief Too many (hard) links to the same filesystem object. */
        TooManyLinks,
        /** @brief A filename was invalid. */
        InvalidFilename,
        /** @brief Program argument list too long. */
        ArgumentListTooLong,
        /** @brief This operation was interrupted. */
        Interrupted,
        /** @brief This operation is unsupported on this platform. */
        Unsupported,
        /** @brief An error returned when an operation could not be completed because an "end of file" was reached prematurely. */
        UnexpectedEof,
        /** @brief An operation could not be completed, because it failed to allocate enough memory. */
        OutOfMemory,
        /** @brief The operation was partially successful and needs to be checked later on due to not blocking. */
        InProgress,
        /** @brief A custom error that does not fall under any other I/O error kind. */
        Other,
    };

    const char *format_error_kind(ErrorKind kind);

    /**
     * @brief Enumeration of possible methods to seek within an I/O object.
     *
     * @see https://doc.rust-lang.org/std/io/enum.SeekFrom.html
     */
    struct SeekFrom
    {
        enum Type
        {
            Start,
            End,
            Current
        } type;
        int64_t offset;
    };

    /**
     * @brief The error type for I/O operations of the @ref Read, @ref Write, @ref Seek, and associated interfaces.
     *
     * @see https://doc.rust-lang.org/std/io/struct.Error.html
     */
    class Error : public NonConstructible, public error::Error
    {
    private:
        ErrorKind _kind;
        std::string _message;

    public:
        /** @brief Creates a new I/O error from a known kind of error as well as an arbitrary error payload. */
        explicit Error(ErrorKind kind, const std::string &message);

        /** @brief Creates a new I/O error from an arbitrary error payload. */
        static Error other(const std::string &message);

        /** @brief Creates a new instance of an @ref Error from a particular OS error code. */
        static Error from_raw_os_error(int code);

        /** @brief Returns an error representing the last OS error which occurred. */
        static Error last_os_error();

        const char *message() const noexcept override;
    };

    template <typename T>
    using Result = result::Result<T, Error>;

    class Read
    {
    public:
        virtual Result<size_t> read(std::span<char> buffer) = 0;
    };

    class Write
    {
    public:
        virtual Result<size_t> write(std::span<const char> buffer) = 0;
        virtual Result<std::monostate> flush() = 0;
    };

    class Seek
    {
    public:
        virtual Result<uint64_t> seek(SeekFrom position) = 0;
    };
}
