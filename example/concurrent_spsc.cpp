// Concurrent single-producer single-consumer example.
//
// One thread produces sequenced messages, another consumes them.
// The ring buffer guarantees lock-free, wait-free data transfer
// between exactly one producer and one consumer thread.

#include <RingBuffer.h>

#include <chrono>
#include <cstdint>
#include <cstdio>
#include <thread>

struct Message
{
    uint32_t sequence;
    uint32_t payload;
};

int main()
{
    static constexpr uint32_t kMessageCount = 1'000'000;
    ms::spsc::RingBuffer<Message, 1024> rb;

    auto startTime = std::chrono::steady_clock::now();

    // ── Producer thread ─────────────────────────────────────────────
    std::thread producer([&]() {
        for (uint32_t i = 0; i < kMessageCount; ++i)
        {
            Message msg{i, i * 7};
            // Spin until space is available — no locks, no syscalls.
            while (!rb.push(msg))
            {
                // Buffer full, wait for consumer to drain.
            }
        }
    });

    // ── Consumer thread ─────────────────────────────────────────────
    uint32_t received = 0;
    uint32_t errors = 0;

    std::thread consumer([&]() {
        for (uint32_t i = 0; i < kMessageCount; ++i)
        {
            Message msg{};
            // Spin until data is available.
            while (!rb.pop(msg))
            {
                // Buffer empty, wait for producer to write.
            }

            if (msg.sequence != i || msg.payload != i * 7)
                ++errors;
            ++received;
        }
    });

    producer.join();
    consumer.join();

    auto endTime = std::chrono::steady_clock::now();
    auto elapsedUs = std::chrono::duration_cast<std::chrono::microseconds>(
                         endTime - startTime)
                         .count();

    double messagesPerSec = static_cast<double>(kMessageCount) /
                            (static_cast<double>(elapsedUs) / 1'000'000.0);

    printf("Transferred %u messages between two threads\n", received);
    printf("Errors:      %u\n", errors);
    printf("Time:        %ld us\n", (long)elapsedUs);
    printf("Throughput:  %.0f messages/sec\n", messagesPerSec);

    return errors > 0 ? 1 : 0;
}
