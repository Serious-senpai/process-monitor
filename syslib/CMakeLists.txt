cmake_minimum_required(VERSION 3.16)
project(SystemLibrary LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)
set(CMAKE_CXX_EXTENSIONS OFF)

file(TO_CMAKE_PATH "${CMAKE_CURRENT_SOURCE_DIR}" SYSLIB_ROOT)
message(STATUS "Got root of syslib: ${SYSLIB_ROOT}")
message(STATUS "CXX flags: ${CMAKE_CXX_FLAGS_RELEASE}")

file(GLOB_RECURSE LIB_SOURCES CONFIGURE_DEPENDS "${SYSLIB_ROOT}/src/*.cpp")

# Exclude platform-specific sources for the other OS
if(WIN32)
    list(FILTER LIB_SOURCES EXCLUDE REGEX "${SYSLIB_ROOT}/src/linux/.*")
elseif(UNIX)
    list(FILTER LIB_SOURCES EXCLUDE REGEX "${SYSLIB_ROOT}/src/win32/.*")
else()
    message(FATAL_ERROR "Unsupported platform")
endif()

message(STATUS "List of source files for library: ${LIB_SOURCES}")

add_library(SystemLibrary "${LIB_SOURCES}")
target_include_directories(SystemLibrary PUBLIC "${SYSLIB_ROOT}/include")
target_precompile_headers(SystemLibrary PRIVATE "$<$<COMPILE_LANGUAGE:CXX>:${SYSLIB_ROOT}/include/pch.hpp>")

if(WIN32)
    target_precompile_headers(SystemLibrary PRIVATE "$<$<COMPILE_LANGUAGE:CXX>:${SYSLIB_ROOT}/include/win32/pch.hpp>")
elseif(UNIX)
    target_precompile_headers(SystemLibrary PRIVATE "$<$<COMPILE_LANGUAGE:CXX>:${SYSLIB_ROOT}/include/linux/pch.hpp>")
endif()

# Verify that the submodule exists (for clarity and better error messages)
if(NOT EXISTS "${SYSLIB_ROOT}/../extern/googletest/CMakeLists.txt")
    message(FATAL_ERROR "GoogleTest submodule not found! Run: git submodule update --init --recursive")
endif()

add_subdirectory("${SYSLIB_ROOT}/tests")
