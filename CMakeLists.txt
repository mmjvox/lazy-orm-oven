cmake_minimum_required(VERSION 3.5)
project(lazy-orm-oven VERSION 0.0.1 LANGUAGES CXX)
add_definitions (-Dappversion="${CMAKE_PROJECT_VERSION}")

include(CheckIncludeFileCXX)

check_include_file_cxx(any HAS_ANY)
check_include_file_cxx(string_view HAS_STRING_VIEW)
check_include_file_cxx(coroutine HAS_COROUTINE)
if (NOT "${CMAKE_CXX_STANDARD}" STREQUAL "")
    # Do nothing
elseif (HAS_ANY AND HAS_STRING_VIEW AND HAS_COROUTINE)
    set(CMAKE_CXX_STANDARD 20)
elseif (HAS_ANY AND HAS_STRING_VIEW)
    set(CMAKE_CXX_STANDARD 17)
else ()
    set(CMAKE_CXX_STANDARD 14)
endif ()

set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include_directories(
    lib/include/
)

add_library(${PROJECT_NAME} STATIC lib/include/VersionInfo.h
    lib/include/Oven.h lib/src/Oven.cpp)


find_package(Drogon CONFIG REQUIRED)
find_package(lazyorm CONFIG REQUIRED)
target_link_libraries(${PROJECT_NAME} PRIVATE Drogon::Drogon ${lazyorm_LIBRARIES})

add_definitions (-DTRANTOR_VERSION="${Trantor_VERSION}")

if (CMAKE_CXX_STANDARD LESS 17)
    message(FATAL_ERROR "c++17 or higher is required")
    add_definitions (-Dcppv="c++14")
elseif (CMAKE_CXX_STANDARD LESS 20)
    add_definitions (-Dcppv="c++17")
    message(STATUS "use c++17")
else ()
    add_definitions (-Dcppv="c++20")
    message(STATUS "use c++20")
endif ()

# aux_source_directory(plugins PLUGIN_SRC)



add_subdirectory(test)
add_subdirectory(test-qt)
