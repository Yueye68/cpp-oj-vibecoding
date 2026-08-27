cmake_minimum_required(VERSION 3.10)
project(oj_tests)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(GTest REQUIRED)

include_directories(
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/src/utils
)

add_executable(test_config test_config.cpp)
target_link_libraries(test_config
    oj_core
    GTest::gtest_main
    pthread
)
gtest_discover_tests(test_config)
