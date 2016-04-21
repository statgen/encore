cmake_minimum_required(VERSION 3.5)
project(gasp-ctrl)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")

set(SOURCE_FILES src/main.cpp)
add_executable(gasp-ctrl ${SOURCE_FILES})