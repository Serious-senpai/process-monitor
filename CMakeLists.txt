cmake_minimum_required(VERSION 3.16)
project(SystemLibrary LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)
set(CMAKE_CXX_EXTENSIONS OFF)

file(TO_CMAKE_PATH "${CMAKE_CURRENT_SOURCE_DIR}" ROOT)
message(STATUS "Got root of repository: ${ROOT}")

file(GLOB_RECURSE LIB_SOURCES CONFIGURE_DEPENDS "${ROOT}/src/*.cpp")

add_executable(pm-client "${ROOT}/src/main.cpp")
# target_include_directories(pm-client PRIVATE "${ROOT}/include")
target_link_libraries(pm-client PRIVATE SystemLibrary)

enable_testing()

add_subdirectory("${ROOT}/extern/googletest")
add_subdirectory("${ROOT}/syslib")
