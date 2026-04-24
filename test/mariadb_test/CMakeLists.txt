cmake_minimum_required(VERSION 3.10)
project(MariaDBConnectionTest)

set(CMAKE_CXX_STANDARD 17)

# Source files
set(SOURCES
    test_mariadb_connection.cpp
)

# Create executable
add_executable(MariaDB_connection_test ${SOURCES})

# Link libraries
target_link_libraries(MariaDB_connection_test
    MariaDB_lib
    pthread
    lazy-orm-oven
    Catch2::Catch2WithMain
)

# Compiler flags
target_compile_options(MariaDB_connection_test PRIVATE -Wall -Wextra)
