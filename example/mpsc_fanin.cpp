// Multiple-producer single-consumer example.
//
// Several producer threads push work items into a shared ring buffer.
// A single consumer thread drains them and verifies nothing was lost.

#include <mpsc/RingBuffer.h>

#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <thread>
#include <vector>

struct WorkItem
{
    uint32_t producerId;
    uint32_t sequence;
};

int main()
{
    static constexpr uint32_t kProducers      = 4;
    static constexpr uint32_t kItemsPerProducer = 250'000;
    static constexpr uint32_t kTotalItems     = kProducers * kItemsPerProducer;

    ouroboros::mpsc::RingBuffer<WorkItem, 1024> rb;

    auto startTime = std::chrono::steady_clock::now();

    // ── Producer threads ────────────────────────────────────────────
    std::vector<std::thread> producers;
    for (uint32_t p = 0; p < kProducers; ++p)
    {
        producers.emplace_back([&rb, p]() {
            for (uint32_t i = 0; i < kItemsPerProducer; ++i)
            {
                WorkItem item{p, i};
                while (!rb.push(item))
                {
                }
            }
        });
    }

    // ── Consumer thread ─────────────────────────────────────────────
    std::atomic<uint32_t> received{0};
    std::thread consumer([&]() {
        for (uint32_t i = 0; i < kTotalItems; ++i)
        {
            WorkItem item{};
            while (!rb.pop(item))
            {
            }
            ++received;
        }
    });

    for (auto &t : producers)
        t.join();
    consumer.join();

    auto endTime = std::chrono::steady_clock::now();
    auto elapsedUs = std::chrono::duration_cast<std::chrono::microseconds>(
                         endTime - startTime)
                         .count();

    double throughput = static_cast<double>(kTotalItems) /
                        (static_cast<double>(elapsedUs) / 1'000'000.0);

    printf("MPSC: %u producers, %u items each\n", kProducers, kItemsPerProducer);
    printf("Received:   %u / %u\n", received.load(), kTotalItems);
    printf("Time:       %ld us\n", (long)elapsedUs);
    printf("Throughput: %.0f items/sec\n", throughput);

    return (received.load() == kTotalItems) ? 0 : 1;
}
