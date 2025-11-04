#pragma once

#include "pch.hpp"

#define OS_CVT(value_type, expr)                                                                                                          \
    {                                                                                                                                     \
        if ((expr) == -1)                                                                                                                 \
        {                                                                                                                                 \
            return io::Result<value_type>::err(io::IoError(io::IoErrorKind::Os, std::format("OS error {}: {}", errno, strerror(errno)))); \
        }                                                                                                                                 \
    }
