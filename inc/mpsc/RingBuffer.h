// MIT License - see LICENSE file for details.
//
// Lock-free Multiple-Producer Single-Consumer ring buffer.
//
// Uses per-slot sequence counters (Vyukov-style bounded queue) for safe
// multi-producer coordination. Producers CAS on head to reserve slots;
// the single consumer reads without CAS.
//
// Same cache-line isolation strategy as the SPSC variant.

#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <type_traits>

namespace ouroboros::mpsc
{

    // ─── Slot with sequence counter ─────────────────────────────────

    template <typename T>
    struct Slot
    {
        std::atomic<uint32_t> sequence;
        T data;
    };

    // ─── RingBuffer ─────────────────────────────────────────────────

    template <typename T, uint32_t Capacity, uint32_t CacheLineSize = 64>
    class RingBuffer
    {
        static_assert(Capacity > 0 && (Capacity & (Capacity - 1)) == 0,
                      "RingBuffer capacity must be a power of 2");
        static_assert(std::is_trivially_copyable_v<T>,
                      "RingBuffer element type must be trivially copyable");
        static_assert(CacheLineSize > sizeof(std::atomic<uint32_t>),
                      "CacheLineSize must be greater than sizeof(atomic<uint32_t>)");
        static_assert(CacheLineSize > 0 && (CacheLineSize & (CacheLineSize - 1)) == 0,
                      "CacheLineSize must be a power of 2");
        static_assert(Capacity < (uint32_t{1} << 31),
                      "Capacity must be less than 2^31 for signed-difference logic");
        static_assert(std::atomic<uint32_t>::is_always_lock_free,
                      "uint32_t atomics must be lock-free on this platform");

        static constexpr uint32_t Mask = Capacity - 1;

    public:
        using value_type = T;
        // ── Layout types ────────────────────────────────────────────

        struct alignas(CacheLineSize) ControlBlock
        {
            std::atomic<uint32_t> head{0};
            char pad1[CacheLineSize - sizeof(std::atomic<uint32_t>)];
            std::atomic<uint32_t> tail{0};
            char pad2[CacheLineSize - sizeof(std::atomic<uint32_t>)];
        };

        // ── Lifecycle ───────────────────────────────────────────────

        RingBuffer()
        {
            for (uint32_t i = 0; i < Capacity; ++i)
                m_slots[i].sequence.store(i, std::memory_order_relaxed);
        }

        RingBuffer(const RingBuffer &) = delete;
        RingBuffer &operator=(const RingBuffer &) = delete;
        RingBuffer(RingBuffer &&) = delete;
        RingBuffer &operator=(RingBuffer &&) = delete;

        // ── Producer (multiple threads) ─────────────────────────────

        [[nodiscard]] bool push(const T &item)
        {
            uint32_t pos = m_ctrl.head.load(std::memory_order_relaxed);

            for (;;)
            {
                Slot<T> &slot = m_slots[pos & Mask];
                uint32_t seq = slot.sequence.load(std::memory_order_acquire);
                auto diff = static_cast<int32_t>(seq - pos);

                if (diff == 0)
                {
                    if (m_ctrl.head.compare_exchange_weak(
                            pos, pos + 1,
                            std::memory_order_acq_rel,
                            std::memory_order_relaxed))
                    {
                        std::memcpy(&slot.data, &item, sizeof(T));
                        slot.sequence.store(pos + 1, std::memory_order_release);
                        return true;
                    }
                    // CAS failed — pos reloaded by compare_exchange_weak
                }
                else if (diff < 0)
                {
                    return false; // full
                }
                else
                {
                    // Another producer advanced head; reload
                    pos = m_ctrl.head.load(std::memory_order_relaxed);
                }
            }
        }

        // ── Consumer (single thread) ────────────────────────────────

        [[nodiscard]] bool pop(T &item)
        {
            uint32_t pos = m_ctrl.tail.load(std::memory_order_relaxed);
            Slot<T> &slot = m_slots[pos & Mask];
            uint32_t seq = slot.sequence.load(std::memory_order_acquire);

            if (seq != pos + 1)
                return false;

            std::memcpy(&item, &slot.data, sizeof(T));
            slot.sequence.store(pos + Capacity, std::memory_order_release);
            m_ctrl.tail.store(pos + 1, std::memory_order_release);
            return true;
        }

        // ── Queries (approximate — producers may have reserved but not published) ─

        [[nodiscard]] uint32_t readAvailable() const
        {
            uint32_t head = m_ctrl.head.load(std::memory_order_acquire);
            uint32_t tail = m_ctrl.tail.load(std::memory_order_relaxed);
            return head - tail;
        }

        [[nodiscard]] uint32_t writeAvailable() const
        {
            uint32_t head = m_ctrl.head.load(std::memory_order_relaxed);
            uint32_t tail = m_ctrl.tail.load(std::memory_order_acquire);
            return Capacity - (head - tail);
        }

        [[nodiscard]] bool isEmpty() const { return readAvailable() == 0; }
        [[nodiscard]] bool isFull() const { return writeAvailable() == 0; }

        static constexpr uint32_t capacity() { return Capacity; }
        static constexpr uint32_t cacheLineSize() { return CacheLineSize; }

        // ── Reset (NOT thread-safe) ─────────────────────────────────

        void reset()
        {
            m_ctrl.head.store(0, std::memory_order_relaxed);
            m_ctrl.tail.store(0, std::memory_order_relaxed);
            for (uint32_t i = 0; i < Capacity; ++i)
                m_slots[i].sequence.store(i, std::memory_order_relaxed);
        }

    private:
        ControlBlock m_ctrl;
        Slot<T> m_slots[Capacity];
    };

} // namespace ouroboros::mpsc
