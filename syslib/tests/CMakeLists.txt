file(GLOB_RECURSE TEST_SOURCES CONFIGURE_DEPENDS ${SYSLIB_ROOT}/tests/*.cpp)

add_executable(SystemLibraryTests ${TEST_SOURCES})
target_compile_definitions(SystemLibraryTests PRIVATE TEST_DIR="${SYSLIB_ROOT}/../build/test_playground")
target_include_directories(SystemLibraryTests PRIVATE "${SYSLIB_ROOT}/include")
target_link_libraries(SystemLibraryTests PRIVATE gtest_main SystemLibrary)
target_precompile_headers(SystemLibraryTests PRIVATE "$<$<COMPILE_LANGUAGE:CXX>:${SYSLIB_ROOT}/include/pch.hpp>")

if(WIN32)
    target_precompile_headers(SystemLibraryTests PRIVATE "$<$<COMPILE_LANGUAGE:CXX>:${SYSLIB_ROOT}/include/win32/pch.hpp>")
endif()

include(GoogleTest)
gtest_discover_tests(SystemLibraryTests)
