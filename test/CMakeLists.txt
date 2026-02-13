# Owns googletest and all test targets.
#
# If it hasn't been cloned, print a helpful message and skip.

if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/vendor/googletest/CMakeLists.txt")
    message(STATUS "ms-ringbuffer: test/vendor/googletest not found, skipping tests. "
                   "Run: git submodule update --init --recursive")
    return()
endif()

set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
add_subdirectory(vendor/googletest)

add_executable(ringbuffer_tests
    PodRingBufferTest.cpp
    StructRingBufferTest.cpp
    ByteRingBufferTest.cpp
    BoundaryTest.cpp
    ConcurrentTest.cpp
)
target_link_libraries(ringbuffer_tests PRIVATE ms-ringbuffer GTest::gtest_main pthread)

include(GoogleTest)
gtest_discover_tests(ringbuffer_tests)
