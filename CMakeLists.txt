cmake_minimum_required(VERSION 3.14)
project(spsc_queue LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(spsc_queue INTERFACE)
target_include_directories(spsc_queue INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/include)

add_executable(spsc_demo src/main.cpp)

find_package(Threads REQUIRED)
target_link_libraries(spsc_demo PRIVATE spsc_queue Threads::Threads)
