#include "sys.hpp"

namespace io
{
    const char *format_error_kind(ErrorKind kind)
    {
        switch (kind)
        {
        case ErrorKind::AddrInUse:
            return "address in use";
        case ErrorKind::AddrNotAvailable:
            return "address not available";
        case ErrorKind::AlreadyExists:
            return "entity already exists";
        case ErrorKind::ArgumentListTooLong:
            return "argument list too long";
        case ErrorKind::BrokenPipe:
            return "broken pipe";
        case ErrorKind::ConnectionAborted:
            return "connection aborted";
        case ErrorKind::ConnectionRefused:
            return "connection refused";
        case ErrorKind::ConnectionReset:
            return "connection reset";
        case ErrorKind::CrossesDevices:
            return "cross-device link or rename";
        case ErrorKind::Deadlock:
            return "deadlock";
        case ErrorKind::DirectoryNotEmpty:
            return "directory not empty";
        case ErrorKind::ExecutableFileBusy:
            return "executable file busy";
        case ErrorKind::FileTooLarge:
            return "file too large";
        case ErrorKind::FilesystemLoop:
            return "filesystem loop or indirection limit (e.g. symlink loop)";
        case ErrorKind::HostUnreachable:
            return "host unreachable";
        case ErrorKind::InProgress:
            return "in progress";
        case ErrorKind::Interrupted:
            return "operation interrupted";
        case ErrorKind::InvalidData:
            return "invalid data";
        case ErrorKind::InvalidFilename:
            return "invalid filename";
        case ErrorKind::InvalidInput:
            return "invalid input parameter";
        case ErrorKind::IsADirectory:
            return "is a directory";
        case ErrorKind::NetworkDown:
            return "network down";
        case ErrorKind::NetworkUnreachable:
            return "network unreachable";
        case ErrorKind::NotADirectory:
            return "not a directory";
        case ErrorKind::NotConnected:
            return "not connected";
        case ErrorKind::NotFound:
            return "entity not found";
        case ErrorKind::NotSeekable:
            return "seek on unseekable file";
        case ErrorKind::Other:
            return "other error";
        case ErrorKind::OutOfMemory:
            return "out of memory";
        case ErrorKind::PermissionDenied:
            return "permission denied";
        case ErrorKind::QuotaExceeded:
            return "quota exceeded";
        case ErrorKind::ReadOnlyFilesystem:
            return "read-only filesystem or storage medium";
        case ErrorKind::ResourceBusy:
            return "resource busy";
        case ErrorKind::StaleNetworkFileHandle:
            return "stale network file handle";
        case ErrorKind::StorageFull:
            return "no storage space";
        case ErrorKind::TimedOut:
            return "timed out";
        case ErrorKind::TooManyLinks:
            return "too many links";
        case ErrorKind::UnexpectedEof:
            return "unexpected end of file";
        case ErrorKind::Unsupported:
            return "unsupported";
        case ErrorKind::WouldBlock:
            return "operation would block";
        case ErrorKind::WriteZero:
            return "write zero";
        default:
            return "unrecognized error";
        }
    }

    Error::Error(ErrorKind kind, const std::string &message)
        : _kind(kind), _message(std::format("{}: {}", message, format_error_kind(kind))), NonConstructible(NonConstructibleTag::TAG) {}

    Error Error::other(const std::string &message)
    {
        return Error(ErrorKind::Other, std::move(message));
    }

    Error Error::from_raw_os_error(int code)
    {
        auto kind = sys::decode_error_kind(code);
        return Error(kind, std::format("OS error {}", code));
    }

    ErrorKind Error::kind() const noexcept
    {
        return _kind;
    }

    const char *Error::message() const noexcept
    {
        return _message.c_str();
    }
}
