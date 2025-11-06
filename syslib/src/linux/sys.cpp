#include "sys.hpp"

namespace sys
{
    io::ErrorKind decode_error_kind(int code)
    {
        switch (code)
        {
        case E2BIG:
            return io::ErrorKind::ArgumentListTooLong;
        case EADDRINUSE:
            return io::ErrorKind::AddrInUse;
        case EADDRNOTAVAIL:
            return io::ErrorKind::AddrNotAvailable;
        case EBUSY:
            return io::ErrorKind::ResourceBusy;
        case ECONNABORTED:
            return io::ErrorKind::ConnectionAborted;
        case ECONNREFUSED:
            return io::ErrorKind::ConnectionRefused;
        case ECONNRESET:
            return io::ErrorKind::ConnectionReset;
        case EDEADLK:
            return io::ErrorKind::Deadlock;
        case EDQUOT:
            return io::ErrorKind::QuotaExceeded;
        case EEXIST:
            return io::ErrorKind::AlreadyExists;
        case EFBIG:
            return io::ErrorKind::FileTooLarge;
        case EHOSTUNREACH:
            return io::ErrorKind::HostUnreachable;
        case EINTR:
            return io::ErrorKind::Interrupted;
        case EINVAL:
            return io::ErrorKind::InvalidInput;
        case EISDIR:
            return io::ErrorKind::IsADirectory;
        case ELOOP:
            return io::ErrorKind::FilesystemLoop;
        case ENOENT:
            return io::ErrorKind::NotFound;
        case ENOMEM:
            return io::ErrorKind::OutOfMemory;
        case ENOSPC:
            return io::ErrorKind::StorageFull;
        case ENOSYS:
            return io::ErrorKind::Unsupported;
        case EMLINK:
            return io::ErrorKind::TooManyLinks;
        case ENAMETOOLONG:
            return io::ErrorKind::InvalidFilename;
        case ENETDOWN:
            return io::ErrorKind::NetworkDown;
        case ENETUNREACH:
            return io::ErrorKind::NetworkUnreachable;
        case ENOTCONN:
            return io::ErrorKind::NotConnected;
        case ENOTDIR:
            return io::ErrorKind::NotADirectory;
        case ENOTEMPTY:
            return io::ErrorKind::DirectoryNotEmpty;
        case EPIPE:
            return io::ErrorKind::BrokenPipe;
        case EROFS:
            return io::ErrorKind::ReadOnlyFilesystem;
        case ESPIPE:
            return io::ErrorKind::NotSeekable;
        case ESTALE:
            return io::ErrorKind::StaleNetworkFileHandle;
        case ETIMEDOUT:
            return io::ErrorKind::TimedOut;
        case ETXTBSY:
            return io::ErrorKind::ExecutableFileBusy;
        case EXDEV:
            return io::ErrorKind::CrossesDevices;
        case EINPROGRESS:
            return io::ErrorKind::InProgress;
        case EOPNOTSUPP:
            return io::ErrorKind::Unsupported;
        case EACCES:
        case EPERM:
            return io::ErrorKind::PermissionDenied;
        default:
            if (code == EAGAIN || code == EWOULDBLOCK)
            {
                return io::ErrorKind::WouldBlock;
            }

            return io::ErrorKind::Other;
        }
    }
}
