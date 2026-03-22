# SPMC Ring Buffer — Design

Lock-free single-producer multiple-consumer ring buffer using per-slot
sequence counters. Structural mirror of MPSC.

## Namespace & File

- `ouroboros::spmc`
- `inc/spmc/RingBuffer.h`

## Template

```cpp
template <typename T, uint32_t Capacity, uint32_t CacheLineSize = 64>
class RingBuffer;
```

Same static asserts as SPSC/MPSC.

## Slot Layout

```cpp
struct Slot {
    std::atomic<uint32_t> sequence;
    T data;
};
```

Same sequence protocol as MPSC:
- `sequence == i`            → writable
- `sequence == i + 1`        → readable
- After consumer reads, set `sequence = i + Capacity`

## ControlBlock

Same as MPSC — head and tail on separate cache lines.

## Producer — push(const T& item)

Single producer, no CAS:

```
  pos = head.load(relaxed)
  slot = &slots[pos & Mask]
  seq = slot->sequence.load(acquire)
  if seq != pos → full, return false
  memcpy item → slot->data
  slot->sequence.store(pos + 1, release)
  head.store(pos + 1, release)
  return true
```

## Consumer — pop(T& item)

Multiple consumers, CAS on tail:

```
loop:
  pos = tail.load(relaxed)
  slot = &slots[pos & Mask]
  seq = slot->sequence.load(acquire)
  diff = (int32_t)(seq - (pos + 1))
  if diff < 0  → empty, return false
  if diff > 0  → another consumer advanced tail, reload
  if diff == 0 → try CAS tail(pos, pos+1, acq_rel, relaxed)
    fail → retry
    success:
      memcpy slot->data → item
      slot->sequence.store(pos + Capacity, release)
      return true
```

## Memory Ordering Rationale

Same pattern as MPSC but mirrored:

| Operation | Ordering | Why |
|-----------|----------|-----|
| head.load (producer) | relaxed | Single producer, own cursor |
| slot.sequence.load (producer) | acquire | Must see consumer's release |
| slot.sequence.store (producer) | release | Publishes data |
| head.store (producer) | release | Publishes progress |
| tail.load (consumer) | relaxed | CAS will validate |
| slot.sequence.load (consumer) | acquire | Must see producer's release |
| tail CAS | acq_rel success, relaxed fail | Claim slot atomically |
| slot.sequence.store (consumer) | release | Frees slot |

## ABA Safety

Same as MPSC — sequence counters prevent ABA.

## API

```cpp
[[nodiscard]] bool push(const T& item);       // single producer only
[[nodiscard]] bool pop(T& item);              // multi-consumer safe
[[nodiscard]] uint32_t readAvailable() const;  // approximate
[[nodiscard]] uint32_t writeAvailable() const; // approximate
[[nodiscard]] bool isEmpty() const;
[[nodiscard]] bool isFull() const;
static constexpr uint32_t capacity();
static constexpr uint32_t cacheLineSize();
void reset();                                 // NOT thread-safe
```

No bulk ops on consumer side.

## Trade-offs vs SPSC

| Concern | SPSC | SPMC |
|---------|------|------|
| Consumer atomics | 1 load + 1 store | CAS loop + sequence store |
| Memory per slot | sizeof(T) | sizeof(atomic<uint32_t>) + sizeof(T) |
| Wait-free? | Yes | No (CAS retries) |
| Bulk read | Yes | No |
| Cache pressure | Minimal | Tail bounces under contention |
