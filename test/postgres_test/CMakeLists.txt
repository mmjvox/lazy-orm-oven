cmake_minimum_required(VERSION 3.5)
project(PostgresConnectionTest CXX)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)


set(SOURCES
    test_postgres_connection.cpp
)

add_executable(Postgres_connection_test ${SOURCES})

target_link_libraries(Postgres_connection_test
    PRIVATE
    pg_lib
    pthread
    lazy-orm-oven
    Catch2::Catch2WithMain
)

target_compile_options(Postgres_connection_test PRIVATE -Wall -Wextra)
