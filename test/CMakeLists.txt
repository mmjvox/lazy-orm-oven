cmake_minimum_required(VERSION 3.5)
project(lazy-orm-oven_test CXX)

# Download Catch2
include(FetchContent)
FetchContent_Declare(
    Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG v3.5.2
)
FetchContent_MakeAvailable(Catch2)



add_executable(${PROJECT_NAME} test_main.cc)

# ##############################################################################
# If you include the drogon source code locally in your project, use this method
# to add drogon 
# target_link_libraries(${PROJECT_NAME} PRIVATE drogon)
#
# and comment out the following lines
target_link_libraries(${PROJECT_NAME} PRIVATE Trantor::Trantor)

if (LAZY_ORM_USE_MARIADB)
    add_subdirectory(mariadb_test)
endif ()

if (LAZY_ORM_USE_POSTGRES)
    add_subdirectory(postgres_test)
endif ()

if (LAZY_ORM_USE_SQLITE3 AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/sqlite_test/CMakeLists.txt")
    add_subdirectory(sqlite_test)
endif ()
