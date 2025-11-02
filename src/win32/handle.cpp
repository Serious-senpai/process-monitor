#include "win32/handle.hpp"

CloseHandleGuard::CloseHandleGuard(HANDLE handle)
    : NonConstructible(NonConstructibleTag::TAG), _handle(handle) {}

CloseHandleGuard::CloseHandleGuard(CloseHandleGuard &&other)
    : NonConstructible(NonConstructibleTag::TAG)
{
    _handle = other._handle;
    other._handle = nullptr;
}

CloseHandleGuard::~CloseHandleGuard()
{
    if (_handle != nullptr && _handle != INVALID_HANDLE_VALUE)
    {
        CloseHandle(_handle);
    }
}

HANDLE CloseHandleGuard::into_handle() && noexcept
{
    HANDLE handle = _handle;
    _handle = nullptr;
    return handle;
}
