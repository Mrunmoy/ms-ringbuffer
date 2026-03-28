# Ouroboros

[![CI](https://github.com/Mrunmoy/Ouroboros/actions/workflows/ci.yml/badge.svg)](https://github.com/Mrunmoy/Ouroboros/actions/workflows/ci.yml)
[![Coverage](https://img.shields.io/endpoint?url=https://mrunmoy.github.io/Ouroboros/badges/coverage.json)](https://mrunmoy.github.io/Ouroboros/coverage/)
[![License: MIT](https://img.shields.io/badge/License-MIT-blue.svg)](LICENSE)
[![Dashboard](https://img.shields.io/badge/Dashboard-Live-darkblue?style=flat)](https://mrunmoy.github.io/Ouroboros/)

Lock-free, cache-friendly ring buffers for C++17 in three flavors: SPSC, MPSC, and SPMC.

Ouroboros is a header-only library built around a single template -- `RingBuffer<T, Capacity, CacheLineSize>` -- that works with any trivially copyable type. The control block separates head and tail onto distinct cache lines to eliminate false sharing, and the entire layout is pointer-free so it can live in shared memory.

## Quick Start

```cpp
#include <spsc/RingBuffer.h>

ouroboros::spsc::RingBuffer<int, 1024> rb;

rb.push(42);

int val;
rb.pop(val);  // val == 42
```

MPSC and SPMC follow the same interface -- just swap the header:

```cpp
#include <mpsc/RingBuffer.h>   // multiple producers, single consumer
#include <spmc/RingBuffer.h>   // single producer, multiple consumers
```

## Variants

All three variants share the same template signature:

```cpp
template <typename T, uint32_t Capacity, uint32_t CacheLineSize = 64>
class RingBuffer;
```

`Capacity` must be a power of 2. `T` must be trivially copyable. Set `CacheLineSize` to 128 for Apple M-series and ARM big cores.

| Concern | SPSC | MPSC / SPMC |
|---|---|---|
| Atomics per op | 1 load + 1 store | 1 CAS loop + sequence store |
| Memory per slot | `sizeof(T)` | `sizeof(atomic<uint32_t>) + sizeof(T)` |
| Bulk ops | Yes (`write`/`read`/`peek`/`skip`, memcpy path) | Not available |
| Wait-free? | Yes | CAS retry loop, lock-free but not wait-free |

The SPSC variant also provides `ByteRingBuffer<N>`, a convenience alias for byte-stream and IPC use cases.

## Architecture

![SPSC Ring Buffer Layout](docs/diagrams/spsc-ring-buffer.png)

The ring uses power-of-two masking on monotonically increasing head/tail counters. The SPSC variant achieves wait-free progress with a single `acquire` load and a single `release` store per operation. MPSC and SPMC use Vyukov-style per-slot sequence counters so that the contended side (multiple producers or multiple consumers) coordinates through CAS on the shared index while the uncontended side stays single-threaded.

For a detailed implementation walkthrough, see [WalkthroughSPSC.md](WalkthroughSPSC.md).

## Performance

The [live dashboard](https://mrunmoy.github.io/Ouroboros/) is updated on every push to `main` and reports:

- SPSC, MPSC, and SPMC throughput (items/sec, GiB/sec)
- ARM cross-compiled code size for Cortex-M4, Cortex-A53, and Cortex-R5
- Native code size breakdown (`text`, `data`, `bss`)
- `sizeof` footprint across multiple configurations
- Line-level test coverage

## Platform Support

| Compiler | Minimum Version | Standard |
|---|---|---|
| GCC | 7+ | C++17 |
| Clang | 5+ | C++17 |
| MSVC | 2017+ | C++17 |

ARM targets (Cortex-M4, A53, R5) are tracked for code size in CI but not tested at runtime.

## Installation

Ouroboros is header-only. The simplest path is copying `inc/spsc/`, `inc/mpsc/`, and `inc/spmc/` into your project's include tree.

**As a CMake subdirectory:**

```bash
git submodule add https://github.com/Mrunmoy/Ouroboros vendor/ouroboros
```

```cmake
add_subdirectory(vendor/ouroboros)
target_link_libraries(your_target PRIVATE ouroboros)
```

Tests and examples are automatically disabled when Ouroboros is consumed as a subdirectory.

## Building from Source

```bash
# Clone with test dependencies:
git clone --recursive https://github.com/Mrunmoy/Ouroboros
cd Ouroboros

# Build script (recommended):
python3 build.py -t           # build + run all tests
python3 build.py -e           # build + examples
python3 build.py -c -t -e    # clean build + tests + examples

# Or use CMake directly:
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)
ctest --test-dir build --output-on-failure
```

The test suite contains 109 unit tests including multi-threaded stress tests. Google Test 1.14.0 is vendored as a submodule under `test/vendor/`.

## Project Layout

```
inc/
  spsc/RingBuffer.h          # SPSC ring buffer (header-only)
  mpsc/RingBuffer.h          # MPSC ring buffer (header-only)
  spmc/RingBuffer.h          # SPMC ring buffer (header-only)
test/                         # 109 unit tests (see test/README.md)
example/                      # Usage examples (see example/README.md)
bench/                        # Throughput and code-size benchmarks
docs/diagrams/                # Architecture diagrams
scripts/                      # CI and dashboard generation
build.py                      # Build driver script
WalkthroughSPSC.md            # Guided SPSC implementation walkthrough
FUTURE.md                     # Roadmap
```

## Documentation

- [WalkthroughSPSC.md](WalkthroughSPSC.md) -- step-by-step guide to the SPSC implementation
- [inc/mpsc/DESIGN.md](inc/mpsc/DESIGN.md) -- MPSC design rationale
- [inc/spmc/DESIGN.md](inc/spmc/DESIGN.md) -- SPMC design rationale
- [doc/architecture-guide.md](doc/architecture-guide.md) -- architecture guide and diagram conventions
- [FUTURE.md](FUTURE.md) -- planned work and open questions

## License

MIT -- see [LICENSE](LICENSE).
