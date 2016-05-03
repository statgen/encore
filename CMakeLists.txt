cmake_minimum_required(VERSION 3.5)
project(gasp-utils)

#set(CMAKE_VERBOSE_MAKEFILE 1)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin)

include_directories("gasp-utils/include")

add_executable(validate-ped
        gasp-utils/src/validate_ped.cpp
        gasp-utils/include/pedigree_validation.hpp)



add_executable(jobstatusd
        gasp-utils/src/jobstatusd.cpp
        gasp-utils/src/job_tracker.cpp
        gasp-utils/include/job_tracker.hpp)

target_include_directories(jobstatusd PRIVATE $ENV{MYSQL_DIR}/include)
target_link_libraries(jobstatusd $ENV{MYSQL_DIR}/lib/libmysqlclient.a)