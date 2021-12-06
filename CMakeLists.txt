cmake_minimum_required(VERSION 3.8)
project ("dint-proxy" CXX)

set(CMAKE_CXX_STANDARD 17)


if (WIN32)
    message(FATAL_ERROR "windows build environment not supported")
endif()

set(CMAKE_CXX_COMPILER g++)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_BINARY_DIR "${CMAKE_SOURCE_DIR}/build")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin")

add_compile_options(-ggdb -O0)

include_directories(BEFORE ${CMAKE_SOURCE_DIR}/)
include_directories(BEFORE ${CMAKE_SOURCE_DIR}/include)

include_directories(BEFORE ${CMAKE_SOURCE_DIR}/lib/)

find_library(CRYPTOPP
    NAMES libcryptopp.a
    HINTS ${CMAKE_SOURCE_DIR}/lib/cryptopp)

find_library(DINT
    NAMES libdint.a
    HINTS ${CMAKE_SOURCE_DIR}/lib/)

add_subdirectory(${CMAKE_SOURCE_DIR}/src)
