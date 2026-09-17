include(FetchContent)
FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG        v1.15.2
)

# Match this project's dynamic CRT so gtest.lib and spsc_tests.exe agree on
# RuntimeLibrary; otherwise MSVC/clang-cl link fails with /failifmismatch.
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

find_package(Threads REQUIRED)

add_executable(spsc_tests
    test_queue_basic.cpp
    test_queue_with_concurrency.cpp
)
target_link_libraries(spsc_tests PRIVATE spsc_queue GTest::gtest_main Threads::Threads)

include(GoogleTest)
gtest_discover_tests(spsc_tests)
