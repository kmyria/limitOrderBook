cmake_minimum_required(VERSION 3.16)
project(mttg LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(GATEWAY_SOURCES
    gateway.cpp
    server.cpp
    orderbook.cpp
)

set(GATEWAY_HEADERS
    server.hpp
    orderbook.hpp
    ringbuffer.hpp
)

add_executable(gateway
    ${GATEWAY_SOURCES}
    ${GATEWAY_HEADERS}
)

add_executable(client
    client.cpp
    client.hpp
)

if (CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(gateway PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(client  PRIVATE -Wall -Wextra -Wpedantic)
elseif (CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    target_compile_options(gateway PRIVATE /W4)
    target_compile_options(client  PRIVATE /W4)
endif()
