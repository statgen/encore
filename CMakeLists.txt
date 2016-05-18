cmake_minimum_required(VERSION 2.8.4)
project(manifold)

include_directories("include" "/opt/local/include")

set(CMAKE_MACOSX_RPATH 1)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wno-unused-local-typedefs -DASIO_STANDALONE -DASIO_HAS_MOVE -std=c++11 -stdlib=libc++")
#set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -DASIO_STANDALONE -std=c++11 -stdlib=libc++")

#set(CMAKE_CXX_COMPILER "/opt/local/bin/g++-mp-4.9")
#set(CMAKE_CC_COMPILER "/opt/local/bin/gcc-mp-4.9")

set(HEADER_FILES
    include/socket.hpp
    include/tcp.hpp
    include/uniform_resource_identifier.hpp
    include/http_error_category.hpp
    include/http_frame.hpp
    include/http_v2_connection.hpp
    include/http_v2_message_head.hpp
    include/http_request_head.hpp
    include/http_v2_response_head.hpp
    include/http_message.hpp
    include/http_response_head.hpp
    include/http_incoming_message.hpp
    include/http_outgoing_message.hpp
    include/http_client.hpp
    include/http_server.hpp
    include/http_router.hpp
    include/hpack.hpp
    include/http_v1_connection.hpp
    include/http_v1_message_head.hpp
    include/http_v1_request_head.hpp
    include/http_v2_request_head.hpp
    include/http_message_head.hpp
    include/http_v1_response_head.hpp
    include/http_stream_client.hpp
    include/http_file_transfer.hpp)

set(SOURCE_FILES
    src/socket.cpp
    src/tcp.cpp
    src/uniform_resource_identifier.cpp
    src/http_error_category.cpp
    src/http_frame.cpp
    src/http_v2_connection.cpp
    src/http_v2_message_head.cpp
    src/http_request_head.cpp
    src/http_v2_response_head.cpp
    src/http_message.cpp
    src/http_incoming_message.cpp
    src/http_outgoing_message.cpp
    src/http_client.cpp
    src/http_server.cpp
    src/http_router.cpp
    src/hpack.cpp
    src/http_v1_connection.cpp
    src/http_v1_message_head.cpp
    src/http_v1_response_head.cpp
    src/http_response_head.cpp
    src/http_v2_request_head.cpp
    src/http_v1_request_head.cpp
    src/http_message_head.cpp
    src/http_stream_client.cpp
    src/http_file_transfer.cpp)

set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_HOME_DIRECTORY}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_HOME_DIRECTORY}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_HOME_DIRECTORY}/bin)

add_library(manifold SHARED ${SOURCE_FILES} ${HEADER_FILES})
target_link_libraries(manifold /opt/local/lib/libssl.dylib)
target_link_libraries(manifold /opt/local/lib/libcrypto.dylib)

add_library(manifold-static STATIC ${SOURCE_FILES} ${HEADER_FILES})

add_executable(manifold-test tests/test.cpp) # ${SOURCE_FILES} ${HEADER_FILES})
target_link_libraries(manifold-test manifold-static)
target_link_libraries(manifold-test /opt/local/lib/libssl.dylib)
target_link_libraries(manifold-test /opt/local/lib/libcrypto.dylib)
#target_link_libraries(manifold /opt/local/lib/libboost_system-mt.a)