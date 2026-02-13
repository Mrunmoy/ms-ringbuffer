# Tests

Unit tests for ms-ringbuffer, using [Google Test](https://github.com/google/googletest) v1.14.0.

## Prerequisites

Google Test is a git submodule under `test/vendor/googletest`. Clone with `--recursive` or run:

```bash
git submodule update --init --recursive
```

## Running tests

```bash
# From the project root:
python3 build.py -t

# Or with CMake directly:
cmake -B build && cmake --build build -j$(nproc)
ctest --test-dir build --output-on-failure
```

## Test files

| File | What it tests |
|------|---------------|
| `PodRingBufferTest.cpp` | Typed tests across 8 POD types (int, uint8_t, uint32_t, uint64_t, int16_t, float, double, char). Each type runs 12 tests: push/pop, bulk, peek, skip, reset, wraparound, fill/drain cycles, interleaved ops, min/max values. |
| `StructRingBufferTest.cpp` | User-defined structs: Point2D, SensorReading, PacketHeader, FixedString (embedded array), NestedStruct. Field integrity through wraparound and bulk operations. |
| `ByteRingBufferTest.cpp` | `ByteRingBuffer<N>` alias for IPC. Raw bytes, length-prefixed framing, variable-length messages, binary data with null bytes. |
| `BoundaryTest.cpp` | Edge cases: capacity=1, capacity=2, empty/full underflow/overflow, exact capacity ops, zero-count no-ops, wraparound at exact boundary, reset mid-stream, cache line alignment, version macros. |
| `ConcurrentTest.cpp` | Multi-threaded SPSC safety: int/uint64/float/char, struct with checksum, bulk batches, asymmetric batches, min capacity (max contention), large 256-byte struct (torn write detection), byte stream IPC, custom 128-byte cache line, 1M-element sustained throughput. |
