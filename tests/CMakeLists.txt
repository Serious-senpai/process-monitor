file(GLOB_RECURSE TEST_SOURCES CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp
)

add_executable(pm-tests ${TEST_SOURCES})

target_link_libraries(pm-tests PRIVATE gtest_main)
target_include_directories(pm-tests PRIVATE ${CMAKE_SOURCE_DIR}/include)
target_precompile_headers(pm-tests PRIVATE "$<$<COMPILE_LANGUAGE:CXX>:${CMAKE_SOURCE_DIR}/include/pch.hpp>")

include(GoogleTest)
gtest_discover_tests(pm-tests)
