cmake_minimum_required(VERSION 3.10)
project(MariaDBConnectionTest)

set(CMAKE_CXX_STANDARD 11)

# Find MariaDB/MySQL client library
find_path(MYSQL_INCLUDE_DIR
    NAMES mysql.h
    PATHS /usr/include/mysql /usr/local/include/mysql
)

find_library(MYSQL_LIBRARY
    NAMES mariadb mysqlclient
    PATHS /usr/lib /usr/local/lib
)

if(NOT MYSQL_INCLUDE_DIR OR NOT MYSQL_LIBRARY)
    message(FATAL_ERROR "MariaDB/MySQL client library not found")
endif()

# Include directories
include_directories(${MYSQL_INCLUDE_DIR} ${CMAKE_CURRENT_SOURCE_DIR})

# Source files
set(SOURCES
    test_mariadb_connection.cpp
)

# Create executable
add_executable(MariaDB_connection_test ${SOURCES})

# Link libraries
target_link_libraries(MariaDB_connection_test ${MYSQL_LIBRARY}
    pthread
    lazy-orm-oven
    Catch2::Catch2WithMain
)

# Compiler flags
target_compile_options(MariaDB_connection_test PRIVATE -Wall -Wextra)
