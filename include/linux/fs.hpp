#pragma once

#include "io.hpp"
#include "pch.hpp"

class _OpenOptions
{
public:
    bool read;
    bool write;
    bool append;
    bool truncate;
    bool create;
    bool create_new;

    int flags;
    mode_t mode;

    explicit _OpenOptions();

    Result<int, IoError> get_access_mode() const;
    Result<int, IoError> get_creation_mode() const;
};

class _File : public NonConstructible
{
public:
    int fd;

    explicit _File(int fd);
    static Result<_File, IoError> open(const char *path, const _OpenOptions &options);
};
