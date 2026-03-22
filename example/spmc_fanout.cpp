// Single-producer multiple-consumer example.
//
// One producer thread generates work items. Multiple consumer threads
// race to claim and process them, demonstrating the SPMC pattern.

#include <spmc/RingBuffer.h>

#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <thread>
#include <vector>

struct Task
{
    uint32_t id;
    uint32_t payload;
};

int main()
{
    static constexpr uint32_t kConsumers  = 4;
    static constexpr uint32_t kTotalItems = 1'000'000;

    ouroboros::spmc::RingBuffer<Task, 1024> rb;

    std::atomic<uint32_t> consumed{0};

    auto startTime = std::chrono::steady_clock::now();

    // ── Consumer threads ────────────────────────────────────────────
    std::vector<std::thread> consumers;
    for (uint32_t c = 0; c < kConsumers; ++c)
    {
        consumers.emplace_back([&]() {
            Task task{};
            while (consumed.load(std::memory_order_relaxed) < kTotalItems)
            {
                if (rb.pop(task))
                    consumed.fetch_add(1, std::memory_order_relaxed);
            }
        });
    }

    // ── Producer thread ─────────────────────────────────────────────
    std::thread producer([&]() {
        for (uint32_t i = 0; i < kTotalItems; ++i)
        {
            Task task{i, i * 3};
            while (!rb.push(task))
            {
            }
        }
    });

    producer.join();
    for (auto &t : consumers)
        t.join();

    auto endTime = std::chrono::steady_clock::now();
    auto elapsedUs = std::chrono::duration_cast<std::chrono::microseconds>(
                         endTime - startTime)
                         .count();

    double throughput = static_cast<double>(consumed.load()) /
                        (static_cast<double>(elapsedUs) / 1'000'000.0);

    printf("SPMC: 1 producer, %u consumers\n", kConsumers);
    printf("Consumed:   %u / %u\n", consumed.load(), kTotalItems);
    printf("Time:       %ld us\n", (long)elapsedUs);
    printf("Throughput: %.0f items/sec\n", throughput);

    return (consumed.load() == kTotalItems) ? 0 : 1;
}
