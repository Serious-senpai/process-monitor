file(GLOB_RECURSE TEST_SOURCES CONFIGURE_DEPENDS ${ROOT}/tests/*.cpp)

message(STATUS "List of source files for tests: ${TEST_SOURCES}")

add_executable(pm-tests ${TEST_SOURCES})
target_compile_definitions(pm-tests PRIVATE TEST_DIR="${ROOT}/build/test_playground")
target_include_directories(pm-tests PRIVATE "${ROOT}/include")
target_link_libraries(pm-tests PRIVATE gtest_main pm-lib)
target_precompile_headers(pm-tests PRIVATE "$<$<COMPILE_LANGUAGE:CXX>:${ROOT}/include/pch.hpp>")

include(GoogleTest)
gtest_discover_tests(pm-tests)
