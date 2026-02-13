cmake_minimum_required(VERSION 3.14)
project(ms-ringbuffer
    LANGUAGES CXX
    DESCRIPTION "Lock-free SPSC ring buffer (header-only, cache-friendly)"
)

# ── Header-only library ─────────────────────────────────────────────
add_library(ms-ringbuffer INTERFACE)
target_include_directories(ms-ringbuffer INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/inc>
    $<INSTALL_INTERFACE:include>
)
target_compile_features(ms-ringbuffer INTERFACE cxx_std_17)

# ── Tests and examples (only when building standalone) ───────────────
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    enable_testing()
    add_subdirectory(test)

    option(MS_RINGBUFFER_BUILD_EXAMPLES "Build examples" OFF)
    if(MS_RINGBUFFER_BUILD_EXAMPLES)
        add_subdirectory(example)
    endif()
endif()
