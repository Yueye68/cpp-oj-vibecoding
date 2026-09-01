cmake_minimum_required(VERSION 3.10)
project(oj_tests)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(GTest REQUIRED)

include_directories(
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/src/utils
    ${CMAKE_SOURCE_DIR}/src/db_pool
    ${CMAKE_SOURCE_DIR}/src/models
    ${MYSQL_INCLUDE_DIRS}
)

add_executable(test_config test_config.cpp)
target_link_libraries(test_config
    oj_core
    GTest::gtest_main
    pthread
)
gtest_discover_tests(test_config)

add_executable(test_logger test_logger.cpp)
target_link_libraries(test_logger
    oj_core
    GTest::gtest_main
    pthread
)
gtest_discover_tests(test_logger)

add_executable(test_connection_pool test_connection_pool.cpp)
target_link_libraries(test_connection_pool
    oj_core
    GTest::gtest_main
    pthread
)
gtest_discover_tests(test_connection_pool)

add_executable(test_models test_models.cpp)
target_link_libraries(test_models
    oj_core
    GTest::gtest_main
    pthread
)
gtest_discover_tests(test_models)

add_executable(test_models_db test_models_db.cpp)
target_link_libraries(test_models_db
    oj_core
    GTest::gtest_main
    pthread
)
gtest_discover_tests(test_models_db)
