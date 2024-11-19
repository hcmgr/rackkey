cmake_minimum_required(VERSION 3.9)
project(rackkey)

# Set C++ standard
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find cpprestsdk package
find_package(cpprestsdk REQUIRED)
find_package(OpenSSL REQUIRED)  # Note the capital letters

# Add executables
add_executable(master master.cpp utils.cpp)
add_executable(hash_ring hash_ring.cpp)

# Link cpprestsdk and OpenSSL libraries
target_link_libraries(master PRIVATE 
    cpprestsdk::cpprest
    OpenSSL::Crypto   # Correct target name
)