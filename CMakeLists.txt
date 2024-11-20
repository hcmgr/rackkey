cmake_minimum_required(VERSION 3.9)
project(rackkey)

# Set C++ standard
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find cpprestsdk package
find_package(cpprestsdk REQUIRED)
find_package(OpenSSL REQUIRED)  # Note the capital letters

# Add executables
add_executable(master master.cpp utils.cpp config.cpp hash_ring.cpp crypto.cpp)

target_link_libraries(master PRIVATE 
    cpprestsdk::cpprest
    OpenSSL::Crypto
)

# Include OpenSSL directories
target_include_directories(master PRIVATE ${OPENSSL_INCLUDE_DIR})