# Future Work — Multi-Producer/Consumer Ring Buffer Variants

## Current State
- SPSC implementation at `inc/spsc/RingBuffer.h` (namespace `ms::spsc`)
- Header-only, lock-free, wait-free, cache-line-padded
- Supports bulk `write()`/`read()`, `peek()`, `skip()`

## Planned Variants

### MPSC (Multiple Producers, Single Consumer) — Moderate
- Consumer side unchanged from SPSC
- Producers CAS-loop on `head` to reserve slots
- Two-phase commit: reserve slot, write data, publish
- Publication options:
  - Per-slot sequence counter (extra `uint32_t` per slot, more memory)
  - Separate "published" watermark (simpler, but serializes publication order)
- Bulk `write()` is hard — contiguous range reservation across wraparound
- CAS contention on `head` causes cache line bouncing under high producer counts
- **Most practical variant to add first** (common pattern: N senders, 1 event loop)

### SPMC (Single Producer, Multiple Consumers) — Moderate
- Mirror of MPSC — consumers CAS on `tail` to claim slots
- Same two-phase issue on read side
- Same per-slot metadata trade-off

### MPMC (Multiple Producers, Multiple Consumers) — Hard
- Both sides need CAS coordination
- Best-known approach: Vyukov's bounded MPMC queue
  - Per-slot `atomic<uint32_t>` sequence counter
  - Producers/consumers check slot sequence to determine writable/readable
  - Adds `sizeof(atomic<uint32_t>)` per slot
- Memory ordering: CAS implies `acq_rel` minimum
- Most libraries (Folly, Boost.Lockfree, rigtorp) implement as separate class

## Trade-offs vs SPSC

| Concern | SPSC | MPSC/SPMC | MPMC |
|---------|------|-----------|------|
| Atomics per op | 1 load + 1 store | 1 CAS loop + slot metadata | 2 CAS loops + slot metadata |
| Memory overhead | 2 atomics total | 2 atomics + per-slot flag | per-slot sequence counter |
| Bulk ops | Simple memcpy | Hard (contiguous reservation) | Very hard |
| Wait-free? | Yes | No (CAS retries) | No |
| Cache pressure | Minimal | Head or tail bounces | Both bounce |

## Design Recommendations
- Implement each variant as a separate class (not a policy template)
- May only offer `push()`/`pop()` for multi-variants (no bulk)
- Keep under `ms::mpsc`, `ms::spmc`, `ms::mpmc` namespaces
- File layout: `inc/mpsc/RingBuffer.h`, `inc/spmc/RingBuffer.h`, `inc/mpmc/RingBuffer.h`
- Start with MPSC — most commonly needed
