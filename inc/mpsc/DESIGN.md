# MPSC Ring Buffer — Design

Lock-free multiple-producer single-consumer ring buffer using per-slot
sequence counters (Vyukov-style bounded queue).

## Namespace & File

- `ouroboros::mpsc`
- `inc/mpsc/RingBuffer.h`

## Template

```cpp
template <typename T, uint32_t Capacity, uint32_t CacheLineSize = 64>
class RingBuffer;
```

Same static asserts as SPSC: power-of-2 capacity, trivially copyable T,
valid CacheLineSize.

## Slot Layout

```cpp
struct Slot {
    std::atomic<uint32_t> sequence;
    T data;
};
```

Sequence protocol (for slot at index `i`):
- `sequence == i`            → writable (producer may claim)
- `sequence == i + 1`        → readable (data published)
- After consumer reads, set `sequence = i + Capacity` to free slot for
  the next lap.

All slots initialized with `sequence[i] = i`.

## ControlBlock

```cpp
struct alignas(CacheLineSize) ControlBlock {
    std::atomic<uint32_t> head{0};
    char pad1[CacheLineSize - sizeof(std::atomic<uint32_t>)];
    std::atomic<uint32_t> tail{0};
    char pad2[CacheLineSize - sizeof(std::atomic<uint32_t>)];
};
```

Same cache-line isolation as SPSC.

## Producer — push(const T& item)

Multiple producers, CAS on head:

```
loop:
  pos = head.load(relaxed)
  slot = &slots[pos & Mask]
  seq = slot->sequence.load(acquire)
  diff = (int32_t)(seq - pos)
  if diff < 0  → full, return false
  if diff > 0  → another producer advanced head, reload
  if diff == 0 → try CAS head(pos, pos+1, acq_rel, relaxed)
    fail → retry
    success:
      memcpy item → slot->data
      slot->sequence.store(pos + 1, release)
      return true
```

## Consumer — pop(T& item)

Single consumer, no CAS needed:

```
  pos = tail.load(relaxed)
  slot = &slots[pos & Mask]
  seq = slot->sequence.load(acquire)
  if seq != pos + 1 → empty, return false
  memcpy slot->data → item
  slot->sequence.store(pos + Capacity, release)
  tail.store(pos + 1, release)
  return true
```

## Memory Ordering Rationale

| Operation | Ordering | Why |
|-----------|----------|-----|
| head.load (producer) | relaxed | Reading own cursor, CAS will validate |
| slot.sequence.load (producer) | acquire | Must see consumer's release store |
| head CAS | acq_rel success, relaxed fail | Acquire latest state, release reservation |
| slot.data write | plain memcpy | Between CAS (acquire) and sequence store (release) |
| slot.sequence.store (producer) | release | Publishes data to consumer |
| tail.load (consumer) | relaxed | Single consumer, own cursor |
| slot.sequence.load (consumer) | acquire | Must see producer's release store |
| slot.data read | plain memcpy | Between sequence load (acquire) and sequence store (release) |
| slot.sequence.store (consumer) | release | Frees slot for producers |
| tail.store (consumer) | release | Publishes progress |

## ABA Safety

The sequence counter prevents ABA:
- A producer can only claim a slot when `seq == pos` (matching the current lap)
- After consumption, sequence jumps to `pos + Capacity`, which won't match
  until head wraps an entire lap
- 32-bit counter wrapping at 2^32 with reasonable capacities makes
  accidental match astronomically unlikely

## API

```cpp
[[nodiscard]] bool push(const T& item);       // multi-producer safe
[[nodiscard]] bool pop(T& item);              // single consumer only
[[nodiscard]] uint32_t readAvailable() const;  // approximate
[[nodiscard]] uint32_t writeAvailable() const; // approximate
[[nodiscard]] bool isEmpty() const;
[[nodiscard]] bool isFull() const;
static constexpr uint32_t capacity();
static constexpr uint32_t cacheLineSize();
void reset();                                 // NOT thread-safe
```

No bulk ops — contiguous range reservation across multiple producers
is impractical.

## Trade-offs vs SPSC

| Concern | SPSC | MPSC |
|---------|------|------|
| Producer atomics | 1 load + 1 store | CAS loop + sequence store |
| Memory per slot | sizeof(T) | sizeof(atomic<uint32_t>) + sizeof(T) |
| Wait-free? | Yes | No (CAS retries) |
| Bulk write | Yes | No |
| Cache pressure | Minimal | Head bounces under contention |
