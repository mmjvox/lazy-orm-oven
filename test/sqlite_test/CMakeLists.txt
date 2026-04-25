cmake_minimum_required(VERSION 3.10)
project(SQLiteConnectionTest)

set(CMAKE_CXX_STANDARD 17)

# Source files
set(SOURCES
    test_sqlite_connection.cpp
)

# Create executable
add_executable(SQLite_connection_test ${SOURCES})

# Link libraries
target_link_libraries(SQLite_connection_test
    SQLite3_lib
    lazy-orm-oven
    Catch2::Catch2WithMain
)

# Compiler flags
target_compile_options(SQLite_connection_test PRIVATE -Wall -Wextra)

# For SQLite amalgamation (if using bundled version)
# target_include_directories(SQLite_connection_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/sqlite)
