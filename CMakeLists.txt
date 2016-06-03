cmake_minimum_required(VERSION 3.5)
project(gasp-utils)

#set(CMAKE_VERBOSE_MAKEFILE 1)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin)

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

include_directories("gasp-utils/include" "../htslib-1.3.1/htslib")



add_executable(vcf-stat gasp-utils/src/vcf_stat.cpp)
set_target_properties(vcf-stat PROPERTIES COMPILE_FLAGS -pthread LINK_FLAGS -pthread)
target_link_libraries(vcf-stat ${CMAKE_SOURCE_DIR}/../htslib-1.3.1/libhts.a)
target_link_libraries(vcf-stat z)

add_executable(validate-ped
               gasp-utils/src/validate_ped.cpp)


add_executable(jobstatusd
               gasp-utils/src/jobstatusd.cpp
               gasp-utils/src/job_tracker.cpp
               gasp-utils/include/job_tracker.hpp)

target_include_directories(jobstatusd PRIVATE ${MYSQL_INCLUDE_DIR})
target_link_libraries(jobstatusd ${MYSQL_LIBRARY})