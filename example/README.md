# Examples

Usage examples for ms-ringbuffer.

## Building

```bash
# From the project root:
python3 build.py -e

# Or with CMake directly:
cmake -B build -DMS_RINGBUFFER_BUILD_EXAMPLES=ON
cmake --build build -j$(nproc)
```

## Running

```bash
./build/example/basic_usage
./build/example/concurrent_spsc
```

## Programs

### basic_usage

Single-threaded demonstration of the full API:
- Push/pop individual elements
- Bulk write/read arrays
- Peek without consuming
- Skip elements
- Struct ring buffer (SensorReading)
- ByteRingBuffer with length-prefixed messages
- Full/empty capacity checks

### concurrent_spsc

Two-thread producer/consumer benchmark:
- Producer pushes 1M sequenced messages
- Consumer pops and verifies ordering and payload integrity
- Reports throughput in messages/sec
- Demonstrates the lock-free, wait-free SPSC guarantee
