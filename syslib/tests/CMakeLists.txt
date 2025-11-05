file(GLOB_RECURSE TEST_SOURCES CONFIGURE_DEPENDS ${SYSLIB_ROOT}/tests/*.cpp)

add_executable(syslib-tests ${TEST_SOURCES})
target_compile_definitions(syslib-tests PRIVATE TEST_DIR="${SYSLIB_ROOT}/../build/test_playground")
target_include_directories(syslib-tests PRIVATE "${SYSLIB_ROOT}/include")
target_link_libraries(syslib-tests PRIVATE gtest_main SystemLibrary)
target_precompile_headers(syslib-tests PRIVATE "$<$<COMPILE_LANGUAGE:CXX>:${SYSLIB_ROOT}/include/pch.hpp>")

if(WIN32)
    target_precompile_headers(syslib-tests PRIVATE "$<$<COMPILE_LANGUAGE:CXX>:${SYSLIB_ROOT}/include/win32/pch.hpp>")
endif()

include(GoogleTest)
gtest_discover_tests(syslib-tests)
