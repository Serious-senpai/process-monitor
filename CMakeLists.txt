cmake_minimum_required(VERSION 3.16)
project(ProcessMonitorClient LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)

file(GLOB_RECURSE SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp)

# Exclude platform-specific sources for the other OS
if(WIN32)
    list(FILTER SOURCES EXCLUDE REGEX ".*/src/linux/.*")
elseif(UNIX)
    list(FILTER SOURCES EXCLUDE REGEX ".*/src/win32/.*")
else()
    message(FATAL_ERROR "Unsupported platform")
endif()

include_directories(include)
add_executable(pm-client ${SOURCES})

target_precompile_headers(pm-client PRIVATE "$<$<COMPILE_LANGUAGE:CXX>:${CMAKE_CURRENT_SOURCE_DIR}/include/pch.hpp>")

# --- Enable tests and link GoogleTest submodule ---
enable_testing()

# Verify that the submodule exists (for clarity and better error messages)
if(NOT EXISTS "${CMAKE_SOURCE_DIR}/extern/googletest/CMakeLists.txt")
    message(FATAL_ERROR "GoogleTest submodule not found! Run: git submodule update --init --recursive")
endif()

add_subdirectory(extern/googletest)
add_subdirectory(tests)
