cmake_minimum_required(VERSION 3.9)
project(rackkey)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
find_package(cpprestsdk REQUIRED)
find_package(OpenSSL REQUIRED)

## Shared
set(SHARED_SRC_DIR "${CMAKE_SOURCE_DIR}/src/shared")

## Master
set(MASTER_SRC_DIR "${CMAKE_SOURCE_DIR}/src/master")
file(GLOB SOURCES
    "${MASTER_SRC_DIR}/*.cpp"
    "${SHARED_SRC_DIR}/*.cpp"
) 
add_executable(master ${SOURCES})
target_link_libraries(master PRIVATE 
    cpprestsdk::cpprest
    OpenSSL::Crypto
)
target_include_directories(master PRIVATE 
    ${OPENSSL_INCLUDE_DIR}
    ${CMAKE_SOURCE_DIR}/src/shared
    ${CMAKE_SOURCE_DIR}/src/master
)

## Storage
# set(STORAGE_SRC_DIR "${CMAKE_SOURCE_DIR}/src/storage")
# file(GLOB SOURCES
#     "${STORAGE_SRC_DIR}/*.cpp"
#     "${SHARED_SRC_DIR}/*.cpp"
# ) 
# add_executable(storage ${SOURCES})
# target_link_libraries(storage PRIVATE 
#     cpprestsdk::cpprest
#     OpenSSL::Crypto
# )
# target_include_directories(storage PRIVATE 
#     ${OPENSSL_INCLUDE_DIR}
#     ${CMAKE_SOURCE_DIR}/src/storage
#     ${CMAKE_SOURCE_DIR}/src/shared
# )

## disk
set(DISK_SRC_DIR "${CMAKE_SOURCE_DIR}/src/storage")
file(GLOB SOURCES
    "${DISK_SRC_DIR}/disk_storage.cpp"
    "${DISK_SRC_DIR}/free_space.cpp"
    "${SHARED_SRC_DIR}/*.cpp"
) 
add_executable(disk ${SOURCES})
target_link_libraries(disk PRIVATE 
    cpprestsdk::cpprest
    OpenSSL::Crypto
)
target_include_directories(disk PRIVATE 
    ${CMAKE_SOURCE_DIR}/src/storage
    ${CMAKE_SOURCE_DIR}/src/shared
)