cmake_minimum_required(VERSION 3.5)
project(gasp-utils)

include(ExternalProject)

#set(CMAKE_VERBOSE_MAKEFILE 1)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin)


set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")

find_path(MYSQL_INCLUDE_DIR
          NAMES "mysql.h"
          PATHS $ENV{MYSQL_INCLUDE_DIR}
          "/usr/include/mysql"
          "/usr/local/include/mysql"
          "/usr/mysql/include/mysql")

find_library(MYSQL_LIBRARY
             NAMES "mysqlclient"
             PATHS $ENV{MYSQL_LIB_DIR}
             "/lib/mysql"
             "/lib64/mysql"
             "/usr/lib/mysql"
             "/usr/lib64/mysql"
             "/usr/local/lib/mysql"
             "/usr/local/lib64/mysql"
             "/usr/mysql/lib/mysql"
             "/usr/mysql/lib64/mysql")

message(WARNING ${MYSQL_INCLUDE_DIR})
message(WARNING ${MYSQL_LIBRARY})

ExternalProject_Add(htslib
        BUILD_IN_SOURCE 1
        URL https://github.com/samtools/htslib/releases/download/1.3.1/htslib-1.3.1.tar.bz2
        #GIT_REPOSITORY "https://github.com/samtools/htslib"
        #GIT_TAG "1.3.1"
        UPDATE_COMMAND ""
        CONFIGURE_COMMAND autoconf && ./configure --prefix=${CMAKE_CURRENT_BINARY_DIR}/htslib
        BUILD_COMMAND make
        INSTALL_COMMAND make install)


include_directories("gasp-utils/include" "${CMAKE_CURRENT_BINARY_DIR}/htslib/include/htslib")


add_executable(vcf-stat gasp-utils/src/vcf_stat.cpp)
add_dependencies(vcf-stat htslib)
set_target_properties(vcf-stat PROPERTIES COMPILE_FLAGS -pthread LINK_FLAGS -pthread)
target_link_libraries(vcf-stat ${CMAKE_CURRENT_BINARY_DIR}/htslib/lib/libhts.a)
target_link_libraries(vcf-stat z)


add_executable(validate-ped
               gasp-utils/src/validate_ped.cpp)


add_executable(jobstatusd
               gasp-utils/src/jobstatusd.cpp
               gasp-utils/src/job_tracker.cpp
               gasp-utils/include/job_tracker.hpp)

target_include_directories(jobstatusd PRIVATE ${MYSQL_INCLUDE_DIR})
target_link_libraries(jobstatusd ${MYSQL_LIBRARY})