# ms-ringbuffer

![Build](https://github.com/Mrunmoy/ms-ringbuffer/actions/workflows/ci.yml/badge.svg?branch=main&event=push) [![Benchmark Dashboard](https://img.shields.io/badge/Benchmark-Live-darkblue?style=flat-square)](https://mrunmoy.github.io/ms-ringbuffer/)

Lock-free, single-producer single-consumer (SPSC) ring buffer for C++17.

Header-only. Cache-friendly. Generic over any trivially copyable type.

## Guided walkthrough

If you want to understand how the SPSC ring buffer is implemented (and why each piece exists), start here:

- [WalkthroughSPSC.md](WalkthroughSPSC.md)

## Benchmark dashboard

Performance and footprint metrics are automatically generated on every
push to `main`.

Live results: https://mrunmoy.github.io/ms-ringbuffer/

Includes:

-   SPSC throughput benchmarks (`uint64_t` and 64-byte payload)
-   Items/sec and GiB/sec metrics
-   Code size report (`text`)
-   RAM usage (`data + bss`)
-   `sizeof` footprint for multiple configurations

------------------------------------------------------------------------

## Features

-   `RingBuffer<T, Capacity, CacheLineSize>` template
-   Single-element API: `push()` / `pop()`
-   Bulk API: `write()` / `read()` / `peek()` / `skip()`
-   `ByteRingBuffer<N>` alias for byte-stream / IPC use
-   Cache-line-padded control block prevents false sharing
    (configurable: 64 or 128 bytes)
-   Designed for shared memory (contiguous layout, no pointers)
-   157 unit tests including multi-threaded stress tests

## Dependencies

-   **C++17** compiler (GCC 7+, Clang 5+, MSVC 2017+)
-   **CMake** 3.14+
-   **Python 3** (for build script, optional)
-   **Google Test** v1.14.0 (bundled as submodule under `test/vendor/`,
    only needed for tests)

## Quick start

``` cpp
#include <spsc/RingBuffer.h>

ms::spsc::RingBuffer<int, 1024> rb;

rb.push(42);

int val;
rb.pop(val);  // val == 42
```

## Building

### Clone

``` bash
# Library only (no tests):
git clone https://github.com/Mrunmoy/ms-ringbuffer

# With tests:
git clone --recursive https://github.com/Mrunmoy/ms-ringbuffer
```

### Build script

``` bash
python3 build.py              # build only
python3 build.py -c           # clean build
python3 build.py -t           # build + run tests
python3 build.py -e           # build + examples
python3 build.py -c -t -e     # clean build + tests + examples
```

### CMake directly

``` bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)

# Run tests:
ctest --test-dir build --output-on-failure

# Build examples:
cmake -B build -DMS_RINGBUFFER_BUILD_EXAMPLES=ON
cmake --build build -j$(nproc)
```

## Using as a submodule

``` bash
git submodule add https://github.com/Mrunmoy/ms-ringbuffer vendor/ms-ringbuffer
```

``` cmake
add_subdirectory(vendor/ms-ringbuffer)
target_link_libraries(your_target PRIVATE ms-ringbuffer)
```

Tests and examples are not built when used as a submodule.

## Project structure

    inc/spsc/RingBuffer.h    # the library (single header)
    test/                    # unit tests (see test/README.md)
    example/                 # usage examples (see example/README.md)
    build.py                 # build script
    LICENSE                  # MIT

## Further reading

-   [test/README.md](test/README.md) - test organization and how to run
-   [example/README.md](example/README.md) - example programs

## License

MIT - see [LICENSE](LICENSE).
