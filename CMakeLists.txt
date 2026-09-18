cmake_minimum_required(VERSION 3.14)
project(spsc_queue LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Header-only library target
add_library(spsc_queue INTERFACE)
target_include_directories(spsc_queue INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/include)

add_executable(spsc_demo src/main.cpp)

# std::thread needs pthread on GCC/Clang
find_package(Threads REQUIRED)
target_link_libraries(spsc_demo PRIVATE spsc_queue Threads::Threads)

# A/B throughput benchmark -- build with --config Release to get real numbers
add_executable(spsc_bench bench/bench_spsc_queue.cpp)
target_link_libraries(spsc_bench PRIVATE spsc_queue Threads::Threads)

# Enable ctest discovery for the Gtests
enable_testing()
add_subdirectory(tests)
