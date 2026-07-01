cmake_minimum_required(VERSION 3.10)
project(mini-reactor)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

add_executable(mini-reactor src/main.cpp src/ThreadPool.cpp)

target_include_directories(mini-reactor PRIVATE include)

find_package(Threads REQUIRED)
target_link_libraries(mini-reactor PRIVATE Threads::Threads)