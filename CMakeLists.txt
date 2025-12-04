cmake_minimum_required(VERSION 3.16)
project(ProcessMonitorClient LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)
set(CMAKE_CXX_EXTENSIONS ON)  # Enable GNU extensions for SHORT_CIRCUIT macro
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -static -static-libgcc -static-libstdc++")

file(TO_CMAKE_PATH "${CMAKE_CURRENT_SOURCE_DIR}" ROOT)
message(STATUS "Got root of repository: ${ROOT}")

file(GLOB_RECURSE LIB_SOURCES CONFIGURE_DEPENDS "${ROOT}/src/*.cpp")

# CTA - Process Monitor Agent
add_executable(CTA "${ROOT}/src/cta.cpp")
target_include_directories(CTA PRIVATE "${ROOT}/src")
target_link_libraries(CTA PRIVATE ProcessMonitor SystemLibrary)
if(WIN32)
    target_link_libraries(CTA PRIVATE psapi)
endif()

# CTB - Process Monitor Log Server
add_executable(CTB "${ROOT}/src/ctb.cpp")
target_include_directories(CTB PRIVATE "${ROOT}/src")
target_link_libraries(CTB PRIVATE SystemLibrary)
if(WIN32)
    target_link_libraries(CTB PRIVATE ws2_32)
endif()

enable_testing()

add_subdirectory("${ROOT}/extern/googletest")
add_subdirectory("${ROOT}/process-monitor")
add_subdirectory("${ROOT}/syslib")
