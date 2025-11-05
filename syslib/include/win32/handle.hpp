#pragma once

#include "pch.hpp"

class CloseHandleGuard : public NonConstructible
{
protected:
    HANDLE _handle;

public:
    explicit CloseHandleGuard(HANDLE handle);
    CloseHandleGuard(CloseHandleGuard &&other);

    virtual ~CloseHandleGuard();

    HANDLE into_handle() && noexcept;
};
