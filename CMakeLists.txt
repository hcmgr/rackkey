cmake_minimum_required(VERSION 3.9)
project(rackkey)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
find_package(cpprestsdk REQUIRED)
find_package(OpenSSL REQUIRED)

## Master
set(MASTER_SRC_DIR "${CMAKE_SOURCE_DIR}/src/master")
file(GLOB SOURCES
    "${MASTER_SRC_DIR}/*.cpp")
add_executable(master ${SOURCES})
target_link_libraries(master PRIVATE 
    cpprestsdk::cpprest
    OpenSSL::Crypto
)
target_include_directories(master PRIVATE ${OPENSSL_INCLUDE_DIR})

## Storage