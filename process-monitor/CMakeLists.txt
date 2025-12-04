cmake_minimum_required(VERSION 3.16)
project(ProcessMonitor LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)
set(CMAKE_CXX_EXTENSIONS ON)  # Enable GNU extensions

file(TO_CMAKE_PATH "${CMAKE_CURRENT_SOURCE_DIR}" PROCESS_MONITOR_ROOT)
message(STATUS "Got root of process-monitor: ${PROCESS_MONITOR_ROOT}")
message(STATUS "CXX flags: ${CMAKE_CXX_FLAGS_RELEASE}")

file(GLOB_RECURSE LIB_SOURCES CONFIGURE_DEPENDS "${PROCESS_MONITOR_ROOT}/src/*.cpp")

# Exclude platform-specific sources for the other OS
if(WIN32)
    list(FILTER LIB_SOURCES EXCLUDE REGEX "${PROCESS_MONITOR_ROOT}/src/linux/.*")
elseif(UNIX)
    list(FILTER LIB_SOURCES EXCLUDE REGEX "${PROCESS_MONITOR_ROOT}/src/win32/.*")
else()
    message(FATAL_ERROR "Unsupported platform")
endif()

message(STATUS "List of source files for library: ${LIB_SOURCES}")

add_library(ProcessMonitor "${LIB_SOURCES}")
target_include_directories(
    ProcessMonitor
    PUBLIC
    "${PROCESS_MONITOR_ROOT}/generated"
    "${PROCESS_MONITOR_ROOT}/include"
)
target_precompile_headers(ProcessMonitor PRIVATE "$<$<COMPILE_LANGUAGE:CXX>:${PROCESS_MONITOR_ROOT}/include/pch.hpp>")

# Rust bindings
if(WIN32)
    file(REAL_PATH "${PROCESS_MONITOR_ROOT}/../windows-listener/target/release/windows_listener.dll" ORIGINAL_CDYLIB)
    file(TO_CMAKE_PATH "${CMAKE_BINARY_DIR}/windows_listener.dll" COPIED_CDYLIB)

elseif(UNIX)
    file(REAL_PATH "${PROCESS_MONITOR_ROOT}/../linux-listener/target/release/liblinux_listener.so" ORIGINAL_CDYLIB)
    file(TO_CMAKE_PATH "${CMAKE_BINARY_DIR}/liblinux_listener.so" COPIED_CDYLIB)

endif()

message(STATUS "Copying ${ORIGINAL_CDYLIB} to ${COPIED_CDYLIB}")
file(COPY_FILE "${ORIGINAL_CDYLIB}" "${COPIED_CDYLIB}")

target_link_libraries(ProcessMonitor PRIVATE "${COPIED_CDYLIB}")
