cmake_minimum_required(VERSION 3.9)
project(rackkey)

# Add compile options
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find required package
find_package(cpprestsdk REQUIRED PATHS)

# Add executable
add_executable(rackkey main.cpp)

# Link libraries
target_link_libraries(rackkey PRIVATE 
    cpprestsdk::cpprest
    crypto
    # ssl
)