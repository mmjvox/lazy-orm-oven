cmake_minimum_required(VERSION 3.5)
project(lazy-orm-oven VERSION 0.0.1 LANGUAGES CXX)

add_definitions(-Dappversion="${CMAKE_PROJECT_VERSION}")

include(CheckIncludeFileCXX)

check_include_file_cxx(any HAS_ANY)
check_include_file_cxx(string_view HAS_STRING_VIEW)
check_include_file_cxx(coroutine HAS_COROUTINE)

if (NOT "${CMAKE_CXX_STANDARD}" STREQUAL "")
    # User defined
elseif (HAS_ANY AND HAS_STRING_VIEW AND HAS_COROUTINE)
    set(CMAKE_CXX_STANDARD 20)
elseif (HAS_ANY AND HAS_STRING_VIEW)
    set(CMAKE_CXX_STANDARD 17)
else ()
    set(CMAKE_CXX_STANDARD 14)
endif ()

set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# -----------------------------------------------------------------------------
# Build options
# -----------------------------------------------------------------------------
option(LAZY_ORM_OVEN_BUILD_TESTS "Build tests" ON)
option(LAZY_ORM_USE_MARIADB "Build MariaDB driver" ON)
option(LAZY_ORM_USE_POSTGRES "Build PostgreSQL driver" ON)
option(LAZY_ORM_USE_SQLITE3 "Build SQLite3 driver" ON)

# -----------------------------------------------------------------------------
# CMake modules
# -----------------------------------------------------------------------------
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake_modules")

# -----------------------------------------------------------------------------
# Dependencies
# -----------------------------------------------------------------------------
find_package(Trantor CONFIG REQUIRED)

if (LAZY_ORM_USE_MARIADB)
    find_package(MariaDB REQUIRED)
endif ()

if (LAZY_ORM_USE_POSTGRES)
    find_package(pg REQUIRED)
endif ()

if (LAZY_ORM_USE_SQLITE3)
    find_package(SQLite3 REQUIRED)
endif ()

# -----------------------------------------------------------------------------
# Sources
# -----------------------------------------------------------------------------
set(LAZY_ORM_PUBLIC_HEADERS
    lib/include/IDbConnection.h
    lib/include/SqlTask.h
)

set(LAZY_ORM_SOURCES
    lib/src/IDbConnection.cpp
    lib/src/SqlTask.cpp
)

set(LAZY_ORM_LIBRARIES
    Trantor::Trantor
)

if (LAZY_ORM_USE_MARIADB)
    list(APPEND LAZY_ORM_PUBLIC_HEADERS
        lib/include/MariadbConnection.h
    )

    list(APPEND LAZY_ORM_SOURCES
        lib/src/MariadbConnection.cpp
    )

    list(APPEND LAZY_ORM_LIBRARIES
        MariaDB_lib
    )

    add_definitions(-DLAZY_ORM_USE_MARIADB)
endif ()

if (LAZY_ORM_USE_POSTGRES)
    list(APPEND LAZY_ORM_PUBLIC_HEADERS
        lib/include/PostgresConnection.h
    )

    list(APPEND LAZY_ORM_SOURCES
        lib/src/PostgresConnection.cpp
    )

    list(APPEND LAZY_ORM_LIBRARIES
        pg_lib
    )

    add_definitions(-DLAZY_ORM_USE_POSTGRES)
endif ()

if (LAZY_ORM_USE_SQLITE3)
    list(APPEND LAZY_ORM_PUBLIC_HEADERS
        lib/include/SqliteConnection.h
    )

    list(APPEND LAZY_ORM_SOURCES
        lib/src/SqliteConnection.cpp
    )

    list(APPEND LAZY_ORM_LIBRARIES
        SQLite3_lib
    )

    add_definitions(-DLAZY_ORM_USE_SQLITE3)
endif ()

add_library(${PROJECT_NAME} STATIC
    ${LAZY_ORM_PUBLIC_HEADERS}
    ${LAZY_ORM_SOURCES}
)

target_include_directories(${PROJECT_NAME}
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/lib/include>
        $<INSTALL_INTERFACE:include>
)

target_link_libraries(${PROJECT_NAME}
    PRIVATE
        ${LAZY_ORM_LIBRARIES}
)

add_definitions(-DTRANTOR_VERSION="${Trantor_VERSION}")

if (CMAKE_CXX_STANDARD LESS 17)
    message(FATAL_ERROR "c++17 or higher is required")
elseif (CMAKE_CXX_STANDARD LESS 20)
    add_definitions(-Dcppv="c++17")
    message(STATUS "use c++17")
else ()
    add_definitions(-Dcppv="c++20")
    message(STATUS "use c++20")
endif ()

message(STATUS "lazy-orm-oven MariaDB driver: ${LAZY_ORM_USE_MARIADB}")
message(STATUS "lazy-orm-oven PostgreSQL driver: ${LAZY_ORM_USE_POSTGRES}")
message(STATUS "lazy-orm-oven SQLite3 driver: ${LAZY_ORM_USE_SQLITE3}")

# aux_source_directory(plugins PLUGIN_SRC)

if (LAZY_ORM_OVEN_BUILD_TESTS)
    add_subdirectory(test)
endif (LAZY_ORM_OVEN_BUILD_TESTS)
