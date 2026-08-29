cmake_minimum_required(VERSION 3.10)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
project(CPP_OJ_System)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

execute_process(
    COMMAND mysql_config --cflags --libs
    OUTPUT_VARIABLE MYSQL_CONFIG_OUTPUT
    OUTPUT_STRIP_TRAILING_WHITESPACE
)

string(REPLACE " " ";" MYSQL_FLAGS_LIST ${MYSQL_CONFIG_OUTPUT})

set(MYSQL_INCLUDE_DIRS "/usr/include/mysql")
set(MYSQL_LIBRARIES "mysqlclient")

find_package(OpenSSL REQUIRED)

include_directories(
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/src/utils
    ${CMAKE_SOURCE_DIR}/src/server
    ${CMAKE_SOURCE_DIR}/src/server/router
    ${CMAKE_SOURCE_DIR}/src/server/handlers
    ${CMAKE_SOURCE_DIR}/src/server/middleware
    ${CMAKE_SOURCE_DIR}/src/models
    ${CMAKE_SOURCE_DIR}/src/services
    ${CMAKE_SOURCE_DIR}/src/db_pool
    ${MYSQL_INCLUDE_DIRS}
    ${OPENSSL_INCLUDE_DIR}
)

set(UTILS_SOURCES
    src/utils/config.cpp
    src/utils/password.cpp
    src/utils/logger.cpp
)

set(MODELS_SOURCES
    src/models/user.cpp
    src/models/problem.cpp
    src/models/test_case.cpp
    src/models/submission.cpp
)

set(SERVICES_SOURCES
    src/services/db_service.cpp
    src/services/judge_service.cpp
    src/services/cgroup_manager.cpp
)

set(DB_POOL_SOURCES
    src/db_pool/connection_pool.cc
)

set(SERVER_SOURCES
    src/server/server.cpp
    src/server/router/router.cpp
    src/server/handlers/auth_handler.cpp
    src/server/handlers/problem_handler.cpp
    src/server/handlers/submission_handler.cpp
    src/server/middleware/auth_middleware.cpp
)

add_library(oj_core STATIC
    ${UTILS_SOURCES}
    ${MODELS_SOURCES}
    ${SERVICES_SOURCES}
    ${DB_POOL_SOURCES}
    ${SERVER_SOURCES}
)

target_link_libraries(oj_core
    ${MYSQL_LIBRARIES}
    OpenSSL::SSL
    OpenSSL::Crypto
    pthread
)

add_executable(oj_server src/main.cpp)
target_link_libraries(oj_server oj_core)

enable_testing()
add_subdirectory(tests/unit)

install(TARGETS oj_server RUNTIME DESTINATION bin)
install(DIRECTORY config/ DESTINATION etc/oj_server/config)
install(DIRECTORY web/ DESTINATION share/oj_server/web)
install(DIRECTORY test_cases/ DESTINATION var/oj_server/test_cases)
install(DIRECTORY uploads/ DESTINATION var/oj_server/uploads)
