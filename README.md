# ouroboros

![Build](https://github.com/Mrunmoy/ouroboros/actions/workflows/ci.yml/badge.svg?branch=main&event=push) [![Benchmark Dashboard](https://img.shields.io/badge/Benchmark-Live-darkblue?style=flat-square)](https://mrunmoy.github.io/ouroboros/)

Lock-free, single-producer single-consumer (SPSC) ring buffer for C++17.

Header-only. Cache-friendly. Generic over any trivially copyable type.

## Guided walkthrough

If you want to understand how the SPSC ring buffer is implemented (and why each piece exists), start here:

- [WalkthroughSPSC.md](WalkthroughSPSC.md)

## Benchmark dashboard

Performance and footprint metrics are automatically generated on every
push to `main`.

Live results: https://mrunmoy.github.io/ouroboros/

Includes:

-   SPSC throughput benchmarks (`uint64_t` and 64-byte payload)
-   Items/sec and GiB/sec metrics
-   Code size report (`text`)
-   RAM usage (`data + bss`)
-   `sizeof` footprint for multiple configurations

------------------------------------------------------------------------

## Features

-   Three lock-free ring buffer variants:
    -   **SPSC** — single-producer single-consumer (wait-free)
    -   **MPSC** — multiple-producer single-consumer
    -   **SPMC** — single-producer multiple-consumer
-   `RingBuffer<T, Capacity, CacheLineSize>` template (all variants)
-   SPSC bulk API: `write()` / `read()` / `peek()` / `skip()`
-   `ByteRingBuffer<N>` alias for byte-stream / IPC use (SPSC)
-   Cache-line-padded control block prevents false sharing
    (configurable: 64 or 128 bytes)
-   Designed for shared memory (contiguous layout, no pointers)
-   193 unit tests including multi-threaded stress tests

## Dependencies

-   **C++17** compiler (GCC 7+, Clang 5+, MSVC 2017+)
-   **CMake** 3.14+
-   **Python 3** (for build script, optional)
-   **Google Test** v1.14.0 (bundled as submodule under `test/vendor/`,
    only needed for tests)

## Quick start

``` cpp
#include <spsc/RingBuffer.h>

ouroboros::spsc::RingBuffer<int, 1024> rb;

rb.push(42);

int val;
rb.pop(val);  // val == 42
```

### MPSC (multiple producers, single consumer)

``` cpp
#include <mpsc/RingBuffer.h>

ouroboros::mpsc::RingBuffer<int, 1024> rb;

// Thread-safe from multiple producers:
rb.push(42);

// Single consumer:
int val;
rb.pop(val);
```

### SPMC (single producer, multiple consumers)

``` cpp
#include <spmc/RingBuffer.h>

ouroboros::spmc::RingBuffer<int, 1024> rb;

// Single producer:
rb.push(42);

// Thread-safe from multiple consumers:
int val;
rb.pop(val);
```

## Building

### Clone

``` bash
# Library only (no tests):
git clone https://github.com/Mrunmoy/ouroboros

# With tests:
git clone --recursive https://github.com/Mrunmoy/ouroboros
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
cmake -B build -DOUROBOROS_BUILD_EXAMPLES=ON
cmake --build build -j$(nproc)
```

## Using as a submodule

``` bash
git submodule add https://github.com/Mrunmoy/ouroboros vendor/ouroboros
```

``` cmake
add_subdirectory(vendor/ouroboros)
target_link_libraries(your_target PRIVATE ouroboros)
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
