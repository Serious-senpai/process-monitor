#pragma once

#include "error.hpp"
#include "pch.hpp"
#include "result.hpp"

enum IoErrorKind
{
    InvalidInput,
    Os,
    Other,
};

class IoError : public NonConstructible, public Error
{
private:
    IoErrorKind _kind;
    std::string _message;

public:
    explicit IoError(IoErrorKind kind, std::string &&message);
    static IoError other(std::string &&message);

    const char *message() const noexcept override;
};

class Read
{
public:
    virtual Result<size_t, IoError> read(std::span<char> buffer) = 0;
};

class Write
{
public:
    virtual Result<size_t, IoError> write(std::span<const char> buffer) = 0;
    virtual Result<std::monostate, IoError> flush() = 0;
};
