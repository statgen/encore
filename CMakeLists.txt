cmake_minimum_required(VERSION 3.5)
project(gasp-utils)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin)

add_executable(validate-ped gasp-utils/src/validate_ped.cpp)