cmake_minimum_required(VERSION 3.16)
project(ProcessMonitorClient LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)
set(CMAKE_CXX_EXTENSIONS ON)  # Enable GNU extensions for SHORT_CIRCUIT macro
if(WIN32)
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -static -static-libgcc -static-libstdc++")
endif()

file(TO_CMAKE_PATH "${CMAKE_CURRENT_SOURCE_DIR}" ROOT)
message(STATUS "Got root of repository: ${ROOT}")

file(GLOB_RECURSE LIB_SOURCES CONFIGURE_DEPENDS "${ROOT}/src/*.cpp")

# Rust bindings
if(WIN32)
    file(REAL_PATH "${ROOT}/windows-listener/target/release/windows_listener.dll" ORIGINAL_CDYLIB)
    file(TO_CMAKE_PATH "${CMAKE_BINARY_DIR}/windows_listener.dll" COPIED_CDYLIB)

elseif(UNIX)
    file(REAL_PATH "${ROOT}/linux-listener/target/release/liblinux_listener.so" ORIGINAL_CDYLIB)
    file(TO_CMAKE_PATH "${CMAKE_BINARY_DIR}/liblinux_listener.so" COPIED_CDYLIB)

endif()

message(STATUS "Copying ${ORIGINAL_CDYLIB} to ${COPIED_CDYLIB}")
file(COPY_FILE "${ORIGINAL_CDYLIB}" "${COPIED_CDYLIB}")

# CTA - Process Monitor Agent
add_executable(CTA "${ROOT}/src/cta.cpp")
target_include_directories(CTA PRIVATE "${ROOT}/src" "${ROOT}/extern/boostorg/json/include")
target_link_libraries(CTA PRIVATE SystemLibrary "${COPIED_CDYLIB}")
# if(WIN32)
#     target_link_libraries(CTA PRIVATE psapi)
# endif()

# CTB - Process Monitor Log Server
add_executable(CTB "${ROOT}/src/ctb.cpp")
target_include_directories(CTB PRIVATE "${ROOT}/src" "${ROOT}/extern/boostorg/json/include")
target_link_libraries(CTB PRIVATE SystemLibrary)
# if(WIN32)
#     target_link_libraries(CTB PRIVATE ws2_32)
# endif()

enable_testing()

add_subdirectory("${ROOT}/extern/googletest")
add_subdirectory("${ROOT}/syslib")
