#pragma once

#include "error.hpp"
#include "pch.hpp"

enum IoErrorKind
{
    Other,
    InvalidInput,
};

class IoError : public Error
{
private:
    IoErrorKind _kind;
    std::string _message;

public:
    explicit IoError(IoErrorKind kind, std::string &&message);
    static IoError other(std::string &&message);

    const char *message() const noexcept override;
};
