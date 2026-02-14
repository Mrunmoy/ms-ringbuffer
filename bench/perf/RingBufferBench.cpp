#include <benchmark/benchmark.h>

#include <atomic>
#include <cstdint>

#include "spsc/RingBuffer.h"

namespace
{

    struct Payload64
    {
        uint8_t m_bytes[64];
    };

    static void ringbuffer_push_pop_u64(benchmark::State &state)
    {
        using namespace ms::spsc;

        RingBuffer<uint64_t, 65536> rb;

        uint64_t value = 0;
        uint64_t out = 0;
        uint64_t ok_count = 0;

        for (auto _ : state)
        {
            benchmark::DoNotOptimize(value);

            if (rb.push(value))
            {
                ok_count++;
            }

            if (rb.pop(out))
            {
                ok_count++;
            }

            benchmark::DoNotOptimize(out);
            benchmark::DoNotOptimize(ok_count);
            benchmark::ClobberMemory();

            value++;
        }
    }

    BENCHMARK(ringbuffer_push_pop_u64);

    // -------------------------
    // SPSC throughput (u64)
    // -------------------------

    template <uint32_t Capacity>
    static void ringbuffer_spsc_throughput_u64_impl(benchmark::State &state)
    {
        using namespace ms::spsc;

        static RingBuffer<uint64_t, Capacity> rb;

        static std::atomic<int> arrived{0};
        static std::atomic<int> generation{0};
        static std::atomic<bool> stop{false};

        const int tid = state.thread_index();
        const int my_gen = generation.load(std::memory_order_acquire);

        if (tid == 0)
        {
            rb.reset();
            stop.store(false, std::memory_order_release);
        }

        if (arrived.fetch_add(1, std::memory_order_acq_rel) == 1)
        {
            arrived.store(0, std::memory_order_release);
            generation.fetch_add(1, std::memory_order_release);
        }
        else
        {
            while (generation.load(std::memory_order_acquire) == my_gen)
            {
            }
        }

        uint64_t local_count = 0;

        if (tid == 0)
        {
            uint64_t v = 0;

            while (state.KeepRunning())
            {
                if (rb.push(v))
                {
                    v++;
                    local_count++;
                }
            }

            stop.store(true, std::memory_order_release);
        }
        else
        {
            uint64_t out = 0;

            while (state.KeepRunning())
            {
                if (rb.pop(out))
                {
                    benchmark::DoNotOptimize(out);
                    local_count++;
                }
            }

            while (!stop.load(std::memory_order_acquire) || !rb.isEmpty())
            {
                if (rb.pop(out))
                {
                    benchmark::DoNotOptimize(out);
                    local_count++;
                }
            }
        }

        state.SetItemsProcessed(static_cast<int64_t>(local_count));
    }

    static void ringbuffer_spsc_throughput_u64(benchmark::State &state)
    {
        const int cap = static_cast<int>(state.range(0));

        if (cap == 1024)
        {
            ringbuffer_spsc_throughput_u64_impl<1024>(state);
            return;
        }

        if (cap == 4096)
        {
            ringbuffer_spsc_throughput_u64_impl<4096>(state);
            return;
        }

        ringbuffer_spsc_throughput_u64_impl<65536>(state);
    }

    BENCHMARK(ringbuffer_spsc_throughput_u64)->Threads(2)->Arg(1024)->Arg(4096)->Arg(65536);

    // -------------------------
    // SPSC throughput (Payload64) - report bytes/sec
    // -------------------------

    template <uint32_t Capacity>
    static void ringbuffer_spsc_throughput_payload64_impl(benchmark::State &state)
    {
        using namespace ms::spsc;

        static RingBuffer<Payload64, Capacity> rb;

        static std::atomic<int> arrived{0};
        static std::atomic<int> generation{0};
        static std::atomic<bool> stop{false};

        const int tid = state.thread_index();
        const int my_gen = generation.load(std::memory_order_acquire);

        if (tid == 0)
        {
            rb.reset();
            stop.store(false, std::memory_order_release);
        }

        if (arrived.fetch_add(1, std::memory_order_acq_rel) == 1)
        {
            arrived.store(0, std::memory_order_release);
            generation.fetch_add(1, std::memory_order_release);
        }
        else
        {
            while (generation.load(std::memory_order_acquire) == my_gen)
            {
            }
        }

        uint64_t local_count = 0;

        if (tid == 0)
        {
            Payload64 v{};
            uint64_t seq = 0;

            while (state.KeepRunning())
            {
                v.m_bytes[0] = static_cast<uint8_t>(seq);

                if (rb.push(v))
                {
                    seq++;
                    local_count++;
                }
            }

            stop.store(true, std::memory_order_release);
        }
        else
        {
            Payload64 out{};

            while (state.KeepRunning())
            {
                if (rb.pop(out))
                {
                    benchmark::DoNotOptimize(out);
                    local_count++;
                }
            }

            while (!stop.load(std::memory_order_acquire) || !rb.isEmpty())
            {
                if (rb.pop(out))
                {
                    benchmark::DoNotOptimize(out);
                    local_count++;
                }
            }
        }

        state.SetItemsProcessed(static_cast<int64_t>(local_count));
        state.SetBytesProcessed(static_cast<int64_t>(local_count * sizeof(Payload64)));
    }

    static void ringbuffer_spsc_throughput_payload64(benchmark::State &state)
    {
        const int cap = static_cast<int>(state.range(0));

        if (cap == 1024)
        {
            ringbuffer_spsc_throughput_payload64_impl<1024>(state);
            return;
        }

        if (cap == 4096)
        {
            ringbuffer_spsc_throughput_payload64_impl<4096>(state);
            return;
        }

        ringbuffer_spsc_throughput_payload64_impl<65536>(state);
    }

    BENCHMARK(ringbuffer_spsc_throughput_payload64)->Threads(2)->Arg(1024)->Arg(4096)->Arg(65536);

} // namespace