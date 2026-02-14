# Walkthrough

This document is a guided walkthrough of the **exact** implementation in `inc/spsc/RingBuffer.h`.
It is intentionally code-driven: each step points at the real lines you ship.

If you only want to _use_ the library, go back to the main README.

---

## 0) What we are building

You describe the goals right at the top of the header:

```cpp
// Lock-free Single-Producer Single-Consumer ring buffer.
//
// Generic template: works with any trivially copyable type T.
// For byte-level IPC / shared memory use, instantiate as RingBuffer.
//
// Cache-friendly design:
// - ControlBlock is aligned to the cache line size (default 64 bytes).
// - Producer (head) and consumer (tail) atomics live on separate cache lines,
// eliminating false sharing between the two cores.
//
// Designed to live in shared memory. The control block (head/tail offsets)
// and data region are laid out contiguously so the entire buffer can be
// placed in a single mmap'd region.
```

Key constraints (we'll justify them as we go):
- **SPSC only** (one producer thread, one consumer thread)
- **`T` must be trivially copyable** (we use `memcpy`)
- **`Capacity` must be a power of two** (we wrap using a mask)

---

## 1) Versioning inside the header

You keep a single source of truth for the library version:

```cpp
struct Version { static constexpr uint8_t major = 1; static constexpr uint8_t minor = 0; static constexpr uint8_t patch = 0; static constexpr uint32_t packed = (major << 16) | (minor << 8) | patch; };
```

This is nice for:
- printing / embedding into benchmark output
- ABI sanity checks when the header is vendored into other repos

---

## 2) Template contract: what the compiler enforces for us

Your template header and compile-time constraints:

```cpp
template <typename T, uint32_t Capacity, uint32_t CacheLineSize = 64>
class RingBuffer
{
    static_assert(Capacity > 0 && (Capacity & (Capacity - 1)) == 0,
                  "RingBuffer capacity must be a power of 2");
    static_assert(std::is_trivially_copyable_v<T>,
                  "RingBuffer element type must be trivially copyable");
    static_assert(CacheLineSize >= sizeof(std::atomic<uint32_t>),
                  "CacheLineSize must be at least sizeof(atomic)");
```

### Why `Capacity` must be power-of-two

Later you compute a constant mask:

```cpp
static constexpr uint32_t Mask = Capacity - 1;
```

and use it like:

```cpp
uint32_t offset = head & Mask;
```

That is only correct wrapping when `Capacity` is power-of-two.

### Why `T` must be trivially copyable

Your fast path is `std::memcpy(...)` (bulk ops), which is only valid for trivially copyable types.

---

## 3) The control block: cache lines and false sharing

This is the core performance trick in your layout.

You define a `ControlBlock` that is aligned, and you explicitly pad between the atomics:

```cpp
struct alignas(CacheLineSize) ControlBlock
{
    std::atomic<uint32_t> head{0};
    char pad1[CacheLineSize - sizeof(std::atomic<uint32_t>)];

    std::atomic<uint32_t> tail{0};
    char pad2[CacheLineSize - sizeof(std::atomic<uint32_t>)];
};
```

### What this buys you

Producer thread writes `head` frequently.
Consumer thread writes `tail` frequently.

If both live on the same cache line, each write invalidates the other core's cache line and you get cache-line ping-pong.

The padding ensures:
- `head` lives alone on one cache line
- `tail` lives alone on another cache line

…and that tends to be worth more than almost any micro-optimization in the rest of the code.

---

## 4) Shared-memory friendliness (contiguous layout)

Your private members:

```cpp
private:
    ControlBlock m_ctrl;
    T m_data[Capacity];
```

This is exactly the "contiguous blob" design you mention in the comments:
- control block first
- data region immediately after
- no pointers, no allocation

If someone wants to place the ring buffer in shared memory, the whole object can be mapped as one region.

---

## 5) Why there are only two atomics (and who owns which)

The SPSC contract lets you keep state minimal:

- `head` is *owned* (written) by producer, read by consumer
- `tail` is *owned* (written) by consumer, read by producer

That ownership pattern shows up in your memory orders.

---

## 6) Reset: the simplest correct thing

```cpp
void reset()
{
    m_ctrl.head.store(0, std::memory_order_relaxed);
    m_ctrl.tail.store(0, std::memory_order_relaxed);
}
```

`reset()` is not a concurrent operation (you don't call it while producer/consumer are running),
so `relaxed` is correct and avoids unnecessary fences.

---

## 7) Producer side: `writeAvailable()`, `push()`, and `write()`

### 7.1 `writeAvailable()`

```cpp
uint32_t writeAvailable() const
{
    uint32_t head = m_ctrl.head.load(std::memory_order_relaxed);
    uint32_t tail = m_ctrl.tail.load(std::memory_order_acquire);
    return Capacity - (head - tail);
}
```

Interpretation:
- `head - tail` is the number of elements currently in the buffer
- capacity minus that is free space

Memory ordering:
- `head` is owned by producer, so `relaxed` is fine for the producer to read its own counter
- `tail` is published by the consumer after consuming data, so the producer uses `acquire` to see it

### 7.2 `push()` delegates to bulk `write()`

```cpp
bool push(const T &item)
{
    return write(&item, 1);
}
```

That keeps the implementation surface small: there is one real write path.

### 7.3 `write()` is the real producer algorithm

```cpp
bool write(const T *data, uint32_t count)
{
    uint32_t head = m_ctrl.head.load(std::memory_order_relaxed);
    uint32_t tail = m_ctrl.tail.load(std::memory_order_acquire);

    if (Capacity - (head - tail) < count)
    {
        return false;
    }

    uint32_t offset = head & Mask;
    uint32_t firstChunk = Capacity - offset;

    if (firstChunk >= count)
    {
        std::memcpy(m_data + offset, data, count * sizeof(T));
    }
    else
    {
        std::memcpy(m_data + offset, data, firstChunk * sizeof(T));
        std::memcpy(m_data, data + firstChunk, (count - firstChunk) * sizeof(T));
    }

    m_ctrl.head.store(head + count, std::memory_order_release);
    return true;
}
```

What’s happening:
- You load `head` (relaxed) and `tail` (acquire).
- You check space with `Capacity - (head - tail)`.
- You compute the write position via `head & Mask`.
- You potentially split the copy into two chunks if wrapping crosses the end of the array.
- You publish the new `head` with a **release** store.

Why `release` on `head.store(...)`:
- This guarantees the `memcpy` writes to `m_data` become visible to the consumer
  **before** the consumer observes the updated `head`.

---

## 8) Consumer side: `readAvailable()`, `pop()`, `peek()`, `read()`, `skip()`

### 8.1 `readAvailable()`

```cpp
uint32_t readAvailable() const
{
    uint32_t head = m_ctrl.head.load(std::memory_order_acquire);
    uint32_t tail = m_ctrl.tail.load(std::memory_order_relaxed);
    return head - tail;
}
```

Symmetric to `writeAvailable()`:
- consumer uses `acquire` when reading producer-owned `head`
- consumer can read its own `tail` with `relaxed`

### 8.2 `pop()` delegates to bulk `read()`

```cpp
bool pop(T &item)
{
    return read(&item, 1);
}
```

Again: one real read path.

### 8.3 `peek()` copies without consuming

```cpp
bool peek(T *dest, uint32_t count) const
{
    uint32_t head = m_ctrl.head.load(std::memory_order_acquire);
    uint32_t tail = m_ctrl.tail.load(std::memory_order_relaxed);

    if (head - tail < count)
    {
        return false;
    }

    uint32_t offset = tail & Mask;
    uint32_t firstChunk = Capacity - offset;

    if (firstChunk >= count)
    {
        std::memcpy(dest, m_data + offset, count * sizeof(T));
    }
    else
    {
        std::memcpy(dest, m_data + offset, firstChunk * sizeof(T));
        std::memcpy(dest + firstChunk, m_data, (count - firstChunk) * sizeof(T));
    }

    return true;
}
```

Important details:
- This is `const`, and it does **not** update `tail`.
- It uses the same wrap/chunk logic as `read()`.

### 8.4 `read()` consumes

```cpp
bool read(T *dest, uint32_t count)
{
    uint32_t head = m_ctrl.head.load(std::memory_order_acquire);
    uint32_t tail = m_ctrl.tail.load(std::memory_order_relaxed);

    if (head - tail < count)
    {
        return false;
    }

    uint32_t offset = tail & Mask;
    uint32_t firstChunk = Capacity - offset;

    if (firstChunk >= count)
    {
        std::memcpy(dest, m_data + offset, count * sizeof(T));
    }
    else
    {
        std::memcpy(dest, m_data + offset, firstChunk * sizeof(T));
        std::memcpy(dest + firstChunk, m_data, (count - firstChunk) * sizeof(T));
    }

    m_ctrl.tail.store(tail + count, std::memory_order_release);
    return true;
}
```

The important line is:

```cpp
m_ctrl.tail.store(tail + count, std::memory_order_release);
```

That `release` pairs with producer `acquire` loads of `tail`, guaranteeing the producer sees the consumer’s progress correctly.

### 8.5 `skip()` consumes without copying

```cpp
bool skip(uint32_t count)
{
    uint32_t head = m_ctrl.head.load(std::memory_order_acquire);
    uint32_t tail = m_ctrl.tail.load(std::memory_order_relaxed);

    if (head - tail < count)
    {
        return false;
    }

    m_ctrl.tail.store(tail + count, std::memory_order_release);
    return true;
}
```

This is useful for "drop messages" style consumers, or framing protocols where you parse headers and decide to discard.

---

## 9) Convenience: capacity, cache line size, empty/full checks

```cpp
static constexpr uint32_t capacity() { return Capacity; }
static constexpr uint32_t cacheLineSize() { return CacheLineSize; }

bool isEmpty() const { return readAvailable() == 0; }
bool isFull() const { return writeAvailable() == 0; }
```

Nothing fancy here — it’s a clean API surface.

---

## 10) Byte ring buffer alias

```cpp
template <uint32_t N>
using ByteRingBuffer = RingBuffer<uint8_t, N>;
```

This is the "IPC building block" version:
- fixed-size byte stream
- easy to layer protocols on top

---

## 11) The concurrency story in one sentence

- Producer writes bytes into `m_data`, then **release-stores** the updated `head`.
- Consumer **acquire-loads** `head` before reading from `m_data`.

…and symmetrically:
- Consumer release-stores updated `tail`.
- Producer acquire-loads `tail` before overwriting `m_data`.

---
