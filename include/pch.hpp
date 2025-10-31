#pragma once

#include <exception>
#include <iostream>
#include <string>
#include <variant>

#ifdef _WIN32
#elif defined(__linux__)
#include <fcntl.h>
#include <sys/types.h>
#endif
