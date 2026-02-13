cmake_minimum_required(VERSION 3.16)
project(mttg LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(GATEWAY_SOURCES
    src/common/gateway.cpp
    src/common/orderbook.cpp
    src/server/server.cpp
)

set(GATEWAY_HEADERS
    src/common/orderbook.hpp
    src/common/ringbuffer.hpp
    src/server/server.hpp
)

add_executable(gateway
    ${GATEWAY_SOURCES}
    ${GATEWAY_HEADERS}
)

add_executable(client
    src/client/client.cpp
    src/client/client.hpp
)

target_include_directories(gateway
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src/server
        ${CMAKE_CURRENT_SOURCE_DIR}/src/client
        ${CMAKE_CURRENT_SOURCE_DIR}/src/common
)

target_include_directories(client
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src/client
)

if (CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(gateway PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(client  PRIVATE -Wall -Wextra -Wpedantic)
elseif (CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    target_compile_options(gateway PRIVATE /W4)
    target_compile_options(client  PRIVATE /W4)
endif()
