cmake_minimum_required(VERSION 3.14)

# -------------------------
# size harness executable
# -------------------------
add_executable(ms_ringbuffer_size
    size/RingBufferSize.cpp
)

target_link_libraries(ms_ringbuffer_size
    PRIVATE
        ms-ringbuffer
)

target_compile_features(ms_ringbuffer_size PRIVATE cxx_std_17)

# Use size-optimized build flags for realistic footprint measurement
if(MSVC)
    target_compile_options(ms_ringbuffer_size PRIVATE /O1)
else()
    target_compile_options(ms_ringbuffer_size PRIVATE
        -Os
        -ffunction-sections
        -fdata-sections
    )
    target_link_options(ms_ringbuffer_size PRIVATE
        -Wl,--gc-sections
        -Wl,-Map=${CMAKE_CURRENT_BINARY_DIR}/ms_ringbuffer_size.map
    )
endif()

# ---- size report (code + ram segments) ----
find_program(SIZE_TOOL size)

if(SIZE_TOOL)
    add_custom_command(
        OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/size_report.txt
        COMMAND ${SIZE_TOOL} $<TARGET_FILE:ms_ringbuffer_size> > ${CMAKE_CURRENT_BINARY_DIR}/size_report.txt
        DEPENDS ms_ringbuffer_size
        VERBATIM
    )

    add_custom_target(ms_ringbuffer_size_report
        DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/size_report.txt
    )
else()
    message(STATUS "size tool not found; ms_ringbuffer_size_report target will not be available")
endif()

# -------------------------
# google benchmark (vendor)
# -------------------------
set(BENCHMARK_ENABLE_TESTING OFF CACHE BOOL "" FORCE)
set(BENCHMARK_ENABLE_GTEST_TESTS OFF CACHE BOOL "" FORCE)
set(BENCHMARK_DOWNLOAD_DEPENDENCIES OFF CACHE BOOL "" FORCE)

add_subdirectory(vendor/benchmark)

# -------------------------
# perf benchmark executable
# -------------------------
add_executable(ms_ringbuffer_bench
    perf/RingBufferBench.cpp
)

target_link_libraries(ms_ringbuffer_bench
    PRIVATE
        ms-ringbuffer
        benchmark
        benchmark_main
)

target_compile_features(ms_ringbuffer_bench PRIVATE cxx_std_17)

if(NOT MSVC)
    target_compile_options(ms_ringbuffer_bench PRIVATE -O3)
endif()