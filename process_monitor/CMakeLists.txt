cmake_minimum_required(VERSION 3.16)
project(ProcessMonitor LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)
set(CMAKE_CXX_EXTENSIONS OFF)

file(TO_CMAKE_PATH "${CMAKE_CURRENT_SOURCE_DIR}" PROCESS_MONITOR_ROOT)
message(STATUS "Got root of process_monitor: ${PROCESS_MONITOR_ROOT}")
message(STATUS "CXX flags: ${CMAKE_CXX_FLAGS_RELEASE}")

file(GLOB_RECURSE LIB_SOURCES CONFIGURE_DEPENDS "${PROCESS_MONITOR_ROOT}/src/*.cpp")

message(STATUS "List of source files for library: ${LIB_SOURCES}")

add_library(ProcessMonitor "${LIB_SOURCES}")
target_include_directories(ProcessMonitor PUBLIC "${PROCESS_MONITOR_ROOT}/include")
target_precompile_headers(ProcessMonitor PRIVATE "$<$<COMPILE_LANGUAGE:CXX>:${PROCESS_MONITOR_ROOT}/include/pch.hpp>")

# Rust bindings
if(WIN32)
    target_link_libraries(ProcessMonitor PRIVATE "${PROCESS_MONITOR_ROOT}/../windows-listener/target/release/windows_listener.dll")
elseif(UNIX)
    target_link_libraries(ProcessMonitor PRIVATE "${PROCESS_MONITOR_ROOT}/../linux-listener/target/release/liblinux_listener.so")
endif()
