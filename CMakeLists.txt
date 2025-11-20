cmake_minimum_required(VERSION 3.16)
project(ProcessMonitorClient LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -static -static-libgcc -static-libstdc++")

file(TO_CMAKE_PATH "${CMAKE_CURRENT_SOURCE_DIR}" ROOT)
message(STATUS "Got root of repository: ${ROOT}")

file(GLOB_RECURSE LIB_SOURCES CONFIGURE_DEPENDS "${ROOT}/src/*.cpp")

add_executable(ProcessMonitorClient "${ROOT}/src/main.cpp")
target_link_libraries(ProcessMonitorClient PRIVATE ProcessMonitor SystemLibrary)

enable_testing()

add_subdirectory("${ROOT}/extern/googletest")
add_subdirectory("${ROOT}/process_monitor")
add_subdirectory("${ROOT}/syslib")
