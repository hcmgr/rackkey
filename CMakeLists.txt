cmake_minimum_required(VERSION 3.9)
project(rackkey)

# Set C++ standard
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find cpprestsdk package
find_package(cpprestsdk REQUIRED PATHS "/usr/lib/x86_64-linux-gnu/cmake/cpprestsdk")

# Add executables
add_executable(master master.cpp)

target_link_libraries(master PRIVATE 
    cpprestsdk::cpprest
    crypto
)
