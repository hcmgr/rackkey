#########################
# STORAGE SERVER CMAKE
#########################
cmake_minimum_required(VERSION 3.10)
project(storage_server)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Locate required packages
find_package(cpprestsdk REQUIRED)
find_package(OpenSSL REQUIRED)

# Define source directories
set(SHARED_SRC_DIR "${CMAKE_SOURCE_DIR}/shared")
set(STORAGE_SRC_DIR "${CMAKE_SOURCE_DIR}/storage")

# Collect source files
file(GLOB SOURCES
    "${STORAGE_SRC_DIR}/*.cpp"
    "${SHARED_SRC_DIR}/*.cpp"
)

add_executable(storage ${SOURCES})

target_link_libraries(storage PRIVATE 
    cpprestsdk::cpprest
    OpenSSL::Crypto
)

target_include_directories(storage PRIVATE 
    ${OPENSSL_INCLUDE_DIR}
    ${SHARED_SRC_DIR}
    ${STORAGE_SRC_DIR}
)
