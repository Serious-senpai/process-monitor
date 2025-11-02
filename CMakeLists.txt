cmake_minimum_required(VERSION 3.16)
project(ProcessMonitorClient LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)
set(CMAKE_CXX_EXTENSIONS OFF)

file(TO_CMAKE_PATH "${CMAKE_CURRENT_SOURCE_DIR}" ROOT)
message(STATUS "Got root of repository: ${ROOT}")

file(GLOB_RECURSE LIB_SOURCES CONFIGURE_DEPENDS "${ROOT}/src/*.cpp")

# Exclude platform-specific sources for the other OS
if(WIN32)
    list(FILTER LIB_SOURCES EXCLUDE REGEX "${ROOT}/src/linux/.*")
elseif(UNIX)
    list(FILTER LIB_SOURCES EXCLUDE REGEX "${ROOT}/src/win32/.*")
else()
    message(FATAL_ERROR "Unsupported platform")
endif()

list(REMOVE_ITEM LIB_SOURCES "${ROOT}/src/main.cpp")

message(STATUS "List of source files for library: ${LIB_SOURCES}")
add_library(pm-lib ${LIB_SOURCES})
target_include_directories(pm-lib PRIVATE "${ROOT}/include")
target_precompile_headers(pm-lib PRIVATE "$<$<COMPILE_LANGUAGE:CXX>:${ROOT}/include/pch.hpp>")

add_executable(pm-client "${ROOT}/src/main.cpp")
target_include_directories(pm-client PRIVATE "${ROOT}/include")
target_link_libraries(pm-client PRIVATE pm-lib)
target_precompile_headers(pm-client PRIVATE "$<$<COMPILE_LANGUAGE:CXX>:${ROOT}/include/pch.hpp>")

# --- Enable tests and link GoogleTest submodule ---
enable_testing()

# Verify that the submodule exists (for clarity and better error messages)
if(NOT EXISTS "${ROOT}/extern/googletest/CMakeLists.txt")
    message(FATAL_ERROR "GoogleTest submodule not found! Run: git submodule update --init --recursive")
endif()

add_subdirectory("${ROOT}/extern/googletest")
add_subdirectory("${ROOT}/tests")
