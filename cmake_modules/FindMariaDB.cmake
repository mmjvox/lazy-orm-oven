# --------------------------------------------------------
# Copyright (C) 1995-2007 MySQL AB
#
# This program is free software; you can redistribute it and/or modify it under
# the terms of version 2 of the GNU General Public License as published by the
# Free Software Foundation.
#
# There are special exceptions to the terms and conditions of the GPL as it is
# applied to this software. View the full text of the exception in file
# LICENSE.exceptions in the top-level directory of this software distribution.
#
# This program is distributed in the hope that it will be useful, but WITHOUT
# ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
# FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
# details.
#
# You should have received a copy of the GNU General Public License along with
# this program; if not, write to the Free Software Foundation, Inc., 51 Franklin
# Street, Fifth Floor, Boston, MA  02110-1301, USA
#
# The MySQL Connector/ODBC is licensed under the terms of the GPL, like most
# MySQL Connectors. There are special exceptions to the terms and conditions of
# the GPL as it is applied to this software, see the FLOSS License Exception
# available on mysql.com.
# MariaDB_lib - The imported target library.

# ##############################################################################

# -------------- FIND MARIADB_INCLUDE_DIRS ------------------
find_path(MARIADB_INCLUDE_DIRS
          NAMES mysql.h
          PATH_SUFFIXES mariadb
          PATHS /usr/include/mysql
                /usr/local/include/mysql
                /usr/include/mariadb
                /usr/local/include/mariadb
                /opt/mysql/mysql/include
                /opt/mysql/mysql/include/mysql
                /opt/mysql/include
                /opt/local/include/mysql5
                /usr/local/mysql/include
                /usr/local/mysql/include/mysql
                /usr/local/mariadb/include
                /usr/local/mariadb/include/mariadb
                /opt/rh/rh-mariadb105/root/usr/include
                /opt/rh/rh-mariadb105/root/usr/include/mysql
                $ENV{ProgramFiles}/MySQL/*/include
                $ENV{SystemDrive}/MySQL/*/include)

find_path(MARIADB_MYSQL_COMPAT_INCLUDE_DIRS
          NAMES mysql.h
          PATH_SUFFIXES mysql
          PATHS /usr/include/mysql
                /usr/local/include/mysql
                /usr/include/mariadb
                /usr/local/include/mariadb
                /opt/mysql/mysql/include
                /opt/mysql/mysql/include/mysql
                /opt/mysql/include
                /opt/local/include/mysql5
                /usr/local/mysql/include
                /usr/local/mysql/include/mysql
                /usr/local/mariadb/include
                /usr/local/mariadb/include/mariadb
                /opt/rh/rh-mariadb105/root/usr/include
                /opt/rh/rh-mariadb105/root/usr/include/mysql
                $ENV{ProgramFiles}/MySQL/*/include
                $ENV{SystemDrive}/MySQL/*/include)

if(EXISTS "${MARIADB_INCLUDE_DIRS}/mysql.h")
elseif(EXISTS "${MARIADB_MYSQL_COMPAT_INCLUDE_DIRS}/mysql.h")
  set(MARIADB_INCLUDE_DIRS ${MARIADB_MYSQL_COMPAT_INCLUDE_DIRS})
elseif(EXISTS "${MARIADB_MYSQL_COMPAT_INCLUDE_DIRS}/mysql/mysql.h")
  set(MARIADB_INCLUDE_DIRS ${MARIADB_MYSQL_COMPAT_INCLUDE_DIRS}/mysql)
endif()

# ----------------- FIND MARIADB_LIBRARIES -------------------
if(WIN32)
  # Set lib path suffixes dist = for binary distributions build = for
  # custom built tree
  if(CMAKE_BUILD_TYPE STREQUAL Debug)
    set(MARIADB_LIB_SUFFIX_DIST debug)
    set(MARIADB_LIB_SUFFIX_BUILD Debug)
  else(CMAKE_BUILD_TYPE STREQUAL Debug)
    set(MARIADB_LIB_SUFFIX_DIST opt)
    set(MARIADB_LIB_SUFFIX_BUILD Release)
    add_definitions(-DDBUG_OFF)
  endif(CMAKE_BUILD_TYPE STREQUAL Debug)

  find_library(MARIADB_LIBRARIES
               NAMES mariadbclient
               PATHS $ENV{MYSQL_DIR}/lib/${MARIADB_LIB_SUFFIX_DIST}
                     $ENV{MYSQL_DIR}/libmysql
                     $ENV{MYSQL_DIR}/libmysql/${MARIADB_LIB_SUFFIX_BUILD}
                     $ENV{MYSQL_DIR}/client/${MARIADB_LIB_SUFFIX_BUILD}
                     $ENV{MYSQL_DIR}/libmysql/${MARIADB_LIB_SUFFIX_BUILD}
                     $ENV{ProgramFiles}/MySQL/*/lib/${MARIADB_LIB_SUFFIX_DIST}
                     $ENV{SystemDrive}/MySQL/*/lib/${MARIADB_LIB_SUFFIX_DIST})
else(WIN32)
  find_library(MARIADB_LIBRARIES
               NAMES mariadbclient mariadb mysqlclient_r
               PATHS /usr/lib/mysql
                     /usr/lib/mariadb
                     /usr/local/lib/mysql
                     /usr/local/lib/mariadb
                     /usr/local/mysql/lib
                     /usr/local/mysql/lib/mysql
                     /opt/local/mysql5/lib
                     /opt/local/lib/mysql5/mysql
                     /opt/mysql/mysql/lib/mysql
                     /opt/mysql/lib/mysql
                     /opt/rh/rh-mariadb105/root/usr/lib64)
endif(WIN32)

if(MARIADB_INCLUDE_DIRS AND MARIADB_LIBRARIES)
  message(STATUS "MariaDB include dir: ${MARIADB_INCLUDE_DIRS}")
  message(STATUS "MariaDB client libraries: ${MARIADB_LIBRARIES}")
elseif(MariaDB_FIND_REQUIRED)
  message(
    FATAL_ERROR
      "Cannot find MariaDB. Include dir: ${MARIADB_INCLUDE_DIRS}  library: ${MARIADB_LIBRARIES}"
    )
endif(MARIADB_INCLUDE_DIRS AND MARIADB_LIBRARIES)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(MariaDB
                                  DEFAULT_MSG
                                  MARIADB_LIBRARIES
                                  MARIADB_INCLUDE_DIRS)
# Copy the results to the output variables.
if(MariaDB_FOUND)
  add_library(MariaDB_lib UNKNOWN IMPORTED)
  set_target_properties(MariaDB_lib PROPERTIES
  INTERFACE_INCLUDE_DIRECTORIES "${MARIADB_INCLUDE_DIRS}"
  IMPORTED_LOCATION "${MARIADB_LIBRARIES}")
  find_package(OpenSSL QUIET)  # try to find openssl
  if(OpenSSL_FOUND)
    target_link_libraries(MariaDB_lib INTERFACE $<LINK_ONLY:OpenSSL::SSL> $<LINK_ONLY:OpenSSL::Crypto>)
    message(STATUS "mariadb: OpenSSL found!")
  else()
    message(STATUS "mariadb: OpenSSL missing!")
  endif()
else(MariaDB_FOUND)
  set(MARIADB_LIBRARIES)
  set(MARIADB_INCLUDE_DIRS)
endif(MariaDB_FOUND)

mark_as_advanced(MARIADB_INCLUDE_DIRS MARIADB_LIBRARIES MARIADB_MYSQL_COMPAT_INCLUDE_DIRS)
