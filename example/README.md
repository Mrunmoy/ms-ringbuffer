# Examples

Usage examples for ouroboros.

## Building

```bash
# From the project root:
python3 build.py -e

# Or with CMake directly:
cmake -B build -DOUROBOROS_BUILD_EXAMPLES=ON
cmake --build build -j$(nproc)
```

## Running

```bash
./build/example/basic_usage
./build/example/concurrent_spsc
./build/example/mpsc_fanin
./build/example/spmc_fanout
```

## Programs

### basic_usage

Single-threaded demonstration of the SPSC API:
- Push/pop individual elements
- Bulk write/read arrays
- Peek without consuming
- Skip elements
- Struct ring buffer (SensorReading)
- ByteRingBuffer with length-prefixed messages
- Full/empty capacity checks

### concurrent_spsc

Two-thread SPSC producer/consumer benchmark:
- Producer pushes 1M sequenced messages
- Consumer pops and verifies ordering and payload integrity
- Reports throughput in messages/sec
- Demonstrates the lock-free, wait-free SPSC guarantee

### mpsc_fanin

Multiple-producer single-consumer fan-in pattern:
- 4 producer threads push 250K items each into a shared ring buffer
- 1 consumer thread drains all items
- Verifies total count and reports throughput
- Demonstrates safe concurrent producer access

### spmc_fanout

Single-producer multiple-consumer fan-out pattern:
- 1 producer thread generates 1M work items
- 4 consumer threads race to claim and process items
- Verifies total consumed count and reports throughput
- Demonstrates safe concurrent consumer access
