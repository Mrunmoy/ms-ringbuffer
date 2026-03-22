cmake_minimum_required(VERSION 3.14)

project(ouroboros
    VERSION 1.0.0
    LANGUAGES CXX
    DESCRIPTION "Lock-free SPSC ring buffer (header-only, cache-friendly)"
)

# ── Header-only library ─────────────────────────────────────────────
configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/cmake/ouroboros_version.h.in
    ${CMAKE_CURRENT_BINARY_DIR}/ouroboros_version.h
    @ONLY
)

add_library(ouroboros INTERFACE)
target_include_directories(ouroboros INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/inc>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>
    $<INSTALL_INTERFACE:include>
)
target_compile_features(ouroboros INTERFACE cxx_std_17)

# ── Tests and examples (only when building standalone) ───────────────
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    enable_testing()
    add_subdirectory(test)

    option(OUROBOROS_BUILD_EXAMPLES "Build examples" OFF)
    if(OUROBOROS_BUILD_EXAMPLES)
        add_subdirectory(example)
    endif()

    option(OUROBOROS_BUILD_BENCHMARKS "Build benchmarks" OFF)
    if(OUROBOROS_BUILD_BENCHMARKS)
        add_subdirectory(bench)
    endif()
endif()