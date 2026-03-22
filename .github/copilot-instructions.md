# Copilot Instructions

Header-only, lock-free ring buffer library for C++17 with three variants:
- **SPSC** — `inc/spsc/RingBuffer.h` (`ouroboros::spsc`) — wait-free, bulk ops
- **MPSC** — `inc/mpsc/RingBuffer.h` (`ouroboros::mpsc`) — CAS producers, push/pop only
- **SPMC** — `inc/spmc/RingBuffer.h` (`ouroboros::spmc`) — CAS consumers, push/pop only

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

- **`inc/spsc/RingBuffer.h`** — SPSC variant. Wait-free. Full API: `push()`/`pop()`, bulk `write()`/`read()`/`peek()`/`skip()`. `ByteRingBuffer<N>` alias.
- **`inc/mpsc/RingBuffer.h`** — MPSC variant. Per-slot sequence counters (Vyukov-style). Producers CAS on head. `push()`/`pop()` only.
- **`inc/spmc/RingBuffer.h`** — SPMC variant. Mirror of MPSC. Consumers CAS on tail. `push()`/`pop()` only.
- **ControlBlock** uses `alignas(CacheLineSize)` with manual padding to prevent false sharing between producer (`head`) and consumer (`tail`) atomics.
- Head/tail are monotonically increasing `uint32_t` atomics, masked with `Capacity - 1` when indexing.

### Design constraints

- Capacity must be a power of 2 (enforced by `static_assert`).
- Element type T must be trivially copyable (enforced by `static_assert`).
- Not copyable or movable — designed for shared memory / mmap'd regions.
- SPSC uses acquire/release ordering (never `seq_cst`). MPSC/SPMC add `acq_rel` CAS.

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
