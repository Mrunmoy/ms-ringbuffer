# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## What this is

Header-only, lock-free SPSC (single-producer single-consumer) ring buffer library for C++17. The entire implementation lives in a single header: `inc/spsc/RingBuffer.h`. Namespace is `ouroboros::spsc`.

## Build commands

Build system uses a Python wrapper (`build.py`) around CMake.

```bash
# Build only (tests are always compiled in standalone mode)
python build.py

# Clean build + run tests
python build.py -c -t

# Build + run tests
python build.py -t

# Build with examples
python build.py -e

# Build with benchmarks + run them
python build.py -b --run-bench

# Full clean build with everything
python build.py -c -t -e -b
```

### Running a single test

```bash
# After building, run a specific test by filter
cd build && ctest --output-on-failure -R "TestName"

# Or run the test binary directly with gtest filter
./build/ringbuffer_tests --gtest_filter="ByteRingBuffer.*"
```

### Direct CMake (without build.py)

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)
ctest --test-dir build --output-on-failure
```

## Architecture

- **`inc/spsc/RingBuffer.h`** — The entire library. Template class `RingBuffer<T, Capacity, CacheLineSize>` with `ByteRingBuffer<N>` alias.
- **`test/`** — Google Test suite (gtest v1.14.0 vendored as submodule in `test/vendor/googletest`). Test binary: `ringbuffer_tests`.
- **`bench/perf/`** — Google Benchmark performance tests (vendored in `bench/vendor/benchmark`).
- **`bench/size/`** — sizeof/code-size measurement harness.
- **`example/`** — Usage examples (`basic_usage.cpp`, `concurrent_spsc.cpp`).

## Key design constraints

- Capacity must be a power of 2 (enforced by static_assert).
- Element type T must be trivially copyable.
- ControlBlock uses cache-line padding to prevent false sharing — `CacheLineSize` template parameter (default 64, use 128 for ARM big cores).
- Head/tail are monotonically increasing `uint32_t` atomics, masked when indexing. This means the buffer wraps naturally at 2^32.
- Not copyable or movable — designed to live in shared memory / mmap'd regions.

## Submodules

Google Test and Google Benchmark are vendored as git submodules. If tests won't build:
```bash
git submodule update --init --recursive
```
