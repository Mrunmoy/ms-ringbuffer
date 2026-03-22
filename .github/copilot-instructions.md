# Copilot Instructions

Header-only, lock-free SPSC (single-producer single-consumer) ring buffer for C++17. The entire library is a single header: `inc/spsc/RingBuffer.h`. Namespace is `ouroboros::spsc`.

## Build commands

Build system uses a Python wrapper (`build.py`) around CMake. Always builds in Release mode.

```bash
python build.py              # build only
python build.py -c -t        # clean build + run tests
python build.py -t           # build + run tests
python build.py -e           # build with examples
python build.py -b --run-bench  # build + run benchmarks
```

### Running a single test

```bash
cd build && ctest --output-on-failure -R "TestName"
# or with gtest filter:
./build/ringbuffer_tests --gtest_filter="ByteRingBuffer.*"
```

### Direct CMake

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)
ctest --test-dir build --output-on-failure
```

CMake options: `OUROBOROS_BUILD_EXAMPLES` (OFF), `OUROBOROS_BUILD_BENCHMARKS` (OFF). Tests always build in standalone mode.

## Architecture

- **`inc/spsc/RingBuffer.h`** — The entire library. Template class `RingBuffer<T, Capacity, CacheLineSize>` with `ByteRingBuffer<N>` alias for byte streams.
- **Producer API**: `push()`, `write()`, `writeAvailable()`
- **Consumer API**: `pop()`, `read()`, `peek()`, `skip()`, `readAvailable()`
- **ControlBlock** uses `alignas(CacheLineSize)` with manual padding to prevent false sharing between producer (`head`) and consumer (`tail`) atomics.
- Head/tail are monotonically increasing `uint32_t` atomics, masked with `Capacity - 1` when indexing. Wraps naturally at 2^32.

### Design constraints

- Capacity must be a power of 2 (enforced by `static_assert`).
- Element type T must be trivially copyable (enforced by `static_assert`).
- Not copyable or movable — designed for shared memory / mmap'd regions.
- Uses strategic `memory_order` (acquire/release pattern, never `seq_cst`).

### Planned expansion (FUTURE.md)

MPSC, SPMC, and MPMC variants will live in separate namespaces (`ms::mpsc`, `ms::spmc`, `ms::mpmc`) and separate headers (`inc/mpsc/RingBuffer.h`, etc.).

## Key conventions

- **Naming**: `PascalCase` classes, `camelCase` methods, `m_` prefix for private members.
- **Include guards**: `#pragma once`.
- **Return values**: `[[nodiscard]] bool` for push/pop/read/write success/failure. No exceptions.
- **Tests**: Google Test (vendored submodule). Fixtures named `PascalCaseTest`. Typed tests for POD types, separate fixtures per struct type.
- **Benchmarks**: Google Benchmark (vendored submodule). Performance benchmarks use `-O3`; size harness uses `-Os`.
- **Section dividers**: Unicode box-drawing characters (`// ─────`).

## Submodules

Google Test and Google Benchmark are vendored as git submodules. If tests won't build:
```bash
git submodule update --init --recursive
```
