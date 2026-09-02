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

add_executable(test_problem_handler test_problem_handler.cpp)
target_link_libraries(test_problem_handler
    oj_core
    GTest::gtest_main
    pthread
)
gtest_discover_tests(test_problem_handler)

add_executable(test_testcase_handler test_testcase_handler.cpp)
target_link_libraries(test_testcase_handler
    oj_core
    GTest::gtest_main
    pthread
)
gtest_discover_tests(test_testcase_handler)

add_executable(test_judge_service test_judge_service.cpp)
target_link_libraries(test_judge_service
    oj_core
    GTest::gtest_main
    pthread
)
gtest_discover_tests(test_judge_service)

add_executable(test_cgroup_manager test_cgroup_manager.cpp)
target_link_libraries(test_cgroup_manager
    oj_core
    GTest::gtest_main
    pthread
)
gtest_discover_tests(test_cgroup_manager)

add_executable(test_session test_session.cpp)
target_link_libraries(test_session
    oj_core
    GTest::gtest_main
    pthread
)
gtest_discover_tests(test_session)

add_executable(test_password test_password.cpp)
target_link_libraries(test_password
    oj_core
    GTest::gtest_main
    pthread
)
gtest_discover_tests(test_password)

add_executable(test_auth_handler test_auth_handler.cpp)
target_link_libraries(test_auth_handler
    oj_core
    GTest::gtest_main
    pthread
)
gtest_discover_tests(test_auth_handler)

gtest_discover_tests(test_models_db)
