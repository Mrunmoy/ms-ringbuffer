// MIT License - see LICENSE file for details.
//
// Lock-free Single-Producer Single-Consumer ring buffer.
//
// Generic template: works with any trivially copyable type T.
// For byte-level IPC / shared memory use, instantiate as RingBuffer<uint8_t, N>.
//
// Cache-friendly design:
//   - ControlBlock is aligned to the cache line size (default 64 bytes).
//   - Producer (head) and consumer (tail) atomics live on separate cache lines,
//     eliminating false sharing between the two cores.
//   - The CacheLineSize template parameter can be tuned for non-standard
//     architectures (e.g. 128 bytes on Apple M-series / ARM big cores).
//
// Designed to live in shared memory. The control block (head/tail offsets)
// and data region are laid out contiguously so the entire buffer can be
// placed in a single mmap'd region.

#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <type_traits>

// Library version.
#define MS_RINGBUFFER_VERSION_MAJOR 1
#define MS_RINGBUFFER_VERSION_MINOR 0
#define MS_RINGBUFFER_VERSION_PATCH 0

namespace ms::spsc
{

    // Lock-free Single-Producer Single-Consumer ring buffer.
    //
    // Template parameters:
    //   T             - Element type. Must be trivially copyable.
    //   Capacity      - Number of elements. Must be a power of 2.
    //   CacheLineSize - Cache line size in bytes (default: 64).
    //                   Controls alignment and padding of the control block
    //                   to prevent false sharing between producer and consumer.
    //                   Use 128 for Apple M-series / ARM big cores.
    template <typename T, uint32_t Capacity, uint32_t CacheLineSize = 64>
    class RingBuffer
    {
        static_assert(Capacity > 0 && (Capacity & (Capacity - 1)) == 0,
                      "RingBuffer capacity must be a power of 2");
        static_assert(std::is_trivially_copyable_v<T>,
                      "RingBuffer element type must be trivially copyable");
        static_assert(CacheLineSize >= sizeof(std::atomic<uint32_t>),
                      "CacheLineSize must be at least sizeof(atomic<uint32_t>)");

    public:
        using value_type = T;
        static constexpr uint32_t Mask = Capacity - 1;

        // Control block - lives at the start of the shared memory region.
        // Offsets are monotonically increasing; masked when indexing into m_data.
        // Each atomic sits on its own cache line to prevent false sharing.
        struct alignas(CacheLineSize) ControlBlock
        {
            std::atomic<uint32_t> head{0};
            char pad1[CacheLineSize - sizeof(std::atomic<uint32_t>)];
            std::atomic<uint32_t> tail{0};
            char pad2[CacheLineSize - sizeof(std::atomic<uint32_t>)];
        };

        RingBuffer() = default;

        // Not copyable or movable (lives in shared memory).
        RingBuffer(const RingBuffer &) = delete;
        RingBuffer &operator=(const RingBuffer &) = delete;

        // Resets the buffer to empty state.
        void reset()
        {
            m_ctrl.head.store(0, std::memory_order_relaxed);
            m_ctrl.tail.store(0, std::memory_order_relaxed);
        }

        // ---------------------------------------------------------------------------
        // Producer API
        // ---------------------------------------------------------------------------

        // Returns the number of elements available for writing.
        uint32_t writeAvailable() const
        {
            uint32_t head = m_ctrl.head.load(std::memory_order_relaxed);
            uint32_t tail = m_ctrl.tail.load(std::memory_order_acquire);
            return Capacity - (head - tail);
        }

        // Pushes a single element into the ring buffer.
        // Returns true on success, false if the buffer is full.
        bool push(const T &item)
        {
            return write(&item, 1);
        }

        // Writes `count` elements from `data` into the ring buffer.
        // Returns true on success, false if insufficient space.
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
                std::memcpy(m_data, data + firstChunk,
                            (count - firstChunk) * sizeof(T));
            }

            m_ctrl.head.store(head + count, std::memory_order_release);
            return true;
        }

        // ---------------------------------------------------------------------------
        // Consumer API
        // ---------------------------------------------------------------------------

        // Returns the number of elements available for reading.
        uint32_t readAvailable() const
        {
            uint32_t head = m_ctrl.head.load(std::memory_order_acquire);
            uint32_t tail = m_ctrl.tail.load(std::memory_order_relaxed);
            return head - tail;
        }

        // Pops a single element from the ring buffer.
        // Returns true on success, false if the buffer is empty.
        bool pop(T &item)
        {
            return read(&item, 1);
        }

        // Peeks at the next `count` elements without consuming them.
        // Returns true if `count` elements are available, false otherwise.
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
                std::memcpy(dest + firstChunk, m_data,
                            (count - firstChunk) * sizeof(T));
            }
            return true;
        }

        // Reads `count` elements from the ring buffer into `dest`.
        // Returns true on success, false if insufficient data.
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
                std::memcpy(dest + firstChunk, m_data,
                            (count - firstChunk) * sizeof(T));
            }

            m_ctrl.tail.store(tail + count, std::memory_order_release);
            return true;
        }

        // Skips `count` elements without reading them.
        // Returns true on success, false if insufficient data.
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

        // ---------------------------------------------------------------------------
        // Capacity / Info
        // ---------------------------------------------------------------------------

        static constexpr uint32_t capacity() { return Capacity; }
        static constexpr uint32_t cacheLineSize() { return CacheLineSize; }

        bool isEmpty() const { return readAvailable() == 0; }
        bool isFull() const { return writeAvailable() == 0; }

    private:
        ControlBlock m_ctrl;
        T m_data[Capacity];
    };

    // Convenience alias for byte-oriented IPC / shared memory use.
    template <uint32_t Size>
    using ByteRingBuffer = RingBuffer<uint8_t, Size>;

} // namespace ms::spsc
