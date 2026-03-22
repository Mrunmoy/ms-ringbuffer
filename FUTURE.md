# Future Work — Multi-Producer/Consumer Ring Buffer Variants

## Implemented

### SPSC (Single Producer, Single Consumer) ✓
- `inc/spsc/RingBuffer.h` (namespace `ouroboros::spsc`)
- Header-only, lock-free, wait-free, cache-line-padded
- Full API: `push()`/`pop()`, bulk `write()`/`read()`, `peek()`, `skip()`

### MPSC (Multiple Producers, Single Consumer) ✓
- `inc/mpsc/RingBuffer.h` (namespace `ouroboros::mpsc`)
- Vyukov-style per-slot sequence counters
- Producers CAS-loop on `head`; consumer reads without CAS
- API: `push()`/`pop()` only (no bulk ops)

### SPMC (Single Producer, Multiple Consumers) ✓
- `inc/spmc/RingBuffer.h` (namespace `ouroboros::spmc`)
- Mirror of MPSC: producer writes without CAS, consumers CAS on `tail`
- API: `push()`/`pop()` only (no bulk ops)

## Trade-offs

| Concern | SPSC | MPSC/SPMC |
|---------|------|-----------|
| Atomics per op | 1 load + 1 store | 1 CAS loop + sequence store |
| Memory per slot | sizeof(T) | sizeof(atomic<uint32_t>) + sizeof(T) |
| Bulk ops | Yes (memcpy) | No |
| Wait-free? | Yes | No (CAS retries) |
| Cache pressure | Minimal | Head or tail bounces |

## Not Planned

### MPMC (Multiple Producers, Multiple Consumers)
- Both sides need CAS coordination (Vyukov bounded queue)
- Marginal performance gain over spinlock queue at low contention
- Well-served by existing libraries (Folly, Boost.Lockfree)
