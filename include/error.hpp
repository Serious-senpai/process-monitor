#pragma once

#include "pch.hpp"

class Error : public std::exception
{
public:
    virtual ~Error() = default;
    virtual const char *message() const noexcept = 0;

    virtual const Error *source() const noexcept;
    friend std::ostream &operator<<(std::ostream &os, const Error &err);
};
