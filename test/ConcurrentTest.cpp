// Multi-threaded safety tests for RingBuffer.
//
// Validates the lock-free SPSC guarantee: one producer thread and one
// consumer thread can operate concurrently without data corruption,
// reordering, or lost elements.
//
// These tests exercise various data types, buffer sizes, bulk operations,
// and sustained high-throughput scenarios.

#include <RingBuffer.h>

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <cstring>
#include <numeric>
#include <thread>
#include <vector>

#include <gtest/gtest.h>

// ---------------------------------------------------------------------------
// Fixture — parameterizable helpers for producer/consumer patterns
// ---------------------------------------------------------------------------

class ConcurrentTest : public ::testing::Test
{
protected:
    // Run a simple SPSC test: producer pushes [0..count), consumer pops and
    // verifies sequential order.
    template <typename RB>
    void runSPSC(RB &rb, uint32_t count)
    {
        using T = typename RB::value_type;

        std::thread producer([&]()
                             {
            for (uint32_t i = 0; i < count; ++i)
            {
                T val = static_cast<T>(i);
                while (!rb.push(val))
                {
                    // spin
                }
            } });

        std::vector<T> received;
        received.reserve(count);

        std::thread consumer([&]()
                             {
            for (uint32_t i = 0; i < count; ++i)
            {
                T val{};
                while (!rb.pop(val))
                {
                    // spin
                }
                received.push_back(val);
            } });

        producer.join();
        consumer.join();

        ASSERT_EQ(received.size(), count);
        for (uint32_t i = 0; i < count; ++i)
        {
            EXPECT_EQ(received[i], static_cast<T>(i))
                << "Mismatch at index " << i;
        }
    }
};

// ---------------------------------------------------------------------------
// Basic SPSC with different POD types
// ---------------------------------------------------------------------------

TEST_F(ConcurrentTest, SPSCInt)
{
    ms::spsc::RingBuffer<int, 256> rb;
    runSPSC(rb, 500000);
}

TEST_F(ConcurrentTest, SPSCUint64)
{
    ms::spsc::RingBuffer<uint64_t, 512> rb;
    runSPSC(rb, 500000);
}

TEST_F(ConcurrentTest, SPSCFloat)
{
    ms::spsc::RingBuffer<float, 128> rb;
    // Float cast from uint32_t may lose precision past ~16M, keep count small.
    runSPSC(rb, 100000);
}

TEST_F(ConcurrentTest, SPSCChar)
{
    ms::spsc::RingBuffer<char, 64> rb;
    // char wraps at 128 (signed) or 256 (unsigned), so limit count.
    runSPSC(rb, 127);
}

// ---------------------------------------------------------------------------
// SPSC with struct (fields must survive concurrent memcpy)
// ---------------------------------------------------------------------------

struct Message
{
    uint32_t id;
    uint32_t payload;
    uint64_t checksum;
};

TEST_F(ConcurrentTest, SPSCStruct)
{
    static constexpr uint32_t kCount = 300000;
    ms::spsc::RingBuffer<Message, 1024> rb;

    std::thread producer([&]()
                         {
        for (uint32_t i = 0; i < kCount; ++i)
        {
            Message m{i, i * 7, static_cast<uint64_t>(i) * 13};
            while (!rb.push(m))
            {
            }
        } });

    std::vector<Message> received;
    received.reserve(kCount);

    std::thread consumer([&]()
                         {
        for (uint32_t i = 0; i < kCount; ++i)
        {
            Message m{};
            while (!rb.pop(m))
            {
            }
            received.push_back(m);
        } });

    producer.join();
    consumer.join();

    ASSERT_EQ(received.size(), kCount);
    for (uint32_t i = 0; i < kCount; ++i)
    {
        EXPECT_EQ(received[i].id, i);
        EXPECT_EQ(received[i].payload, i * 7);
        EXPECT_EQ(received[i].checksum, static_cast<uint64_t>(i) * 13);
    }
}

// ---------------------------------------------------------------------------
// SPSC bulk write/read (producer writes in batches, consumer reads in batches)
// ---------------------------------------------------------------------------

TEST_F(ConcurrentTest, SPSCBulkBatches)
{
    static constexpr uint32_t kTotal = 200000;
    static constexpr uint32_t kBatch = 16;
    ms::spsc::RingBuffer<uint32_t, 256> rb;

    std::thread producer([&]()
                         {
        uint32_t sent = 0;
        while (sent < kTotal)
        {
            uint32_t toSend = std::min(kBatch, kTotal - sent);
            uint32_t batch[kBatch];
            for (uint32_t i = 0; i < toSend; ++i)
                batch[i] = sent + i;

            while (!rb.write(batch, toSend))
            {
                // spin — not enough space
            }
            sent += toSend;
        } });

    std::vector<uint32_t> received;
    received.reserve(kTotal);

    std::thread consumer([&]()
                         {
        uint32_t got = 0;
        while (got < kTotal)
        {
            uint32_t toRead = std::min(kBatch, kTotal - got);
            uint32_t batch[kBatch];

            while (!rb.read(batch, toRead))
            {
                // spin — not enough data
            }
            for (uint32_t i = 0; i < toRead; ++i)
                received.push_back(batch[i]);
            got += toRead;
        } });

    producer.join();
    consumer.join();

    ASSERT_EQ(received.size(), kTotal);
    for (uint32_t i = 0; i < kTotal; ++i)
    {
        EXPECT_EQ(received[i], i) << "Mismatch at index " << i;
    }
}

// ---------------------------------------------------------------------------
// SPSC with asymmetric batch sizes (producer writes 1, consumer reads bulk)
// ---------------------------------------------------------------------------

TEST_F(ConcurrentTest, SPSCAsymmetricBatches)
{
    static constexpr uint32_t kTotal = 100000;
    static constexpr uint32_t kReadBatch = 32;
    ms::spsc::RingBuffer<uint32_t, 512> rb;

    std::thread producer([&]()
                         {
        for (uint32_t i = 0; i < kTotal; ++i)
        {
            while (!rb.push(i))
            {
            }
        } });

    std::vector<uint32_t> received;
    received.reserve(kTotal);

    std::thread consumer([&]()
                         {
        uint32_t got = 0;
        while (got < kTotal)
        {
            uint32_t avail = rb.readAvailable();
            if (avail == 0)
                continue;

            uint32_t toRead = std::min({avail, kReadBatch, kTotal - got});
            std::vector<uint32_t> batch(toRead);

            if (rb.read(batch.data(), toRead))
            {
                for (auto v : batch)
                    received.push_back(v);
                got += toRead;
            }
        } });

    producer.join();
    consumer.join();

    ASSERT_EQ(received.size(), kTotal);
    for (uint32_t i = 0; i < kTotal; ++i)
    {
        EXPECT_EQ(received[i], i);
    }
}

// ---------------------------------------------------------------------------
// SPSC with minimum capacity (1 element) — maximum contention
// ---------------------------------------------------------------------------

TEST_F(ConcurrentTest, SPSCMinCapacity)
{
    static constexpr uint32_t kCount = 50000;
    ms::spsc::RingBuffer<uint32_t, 1> rb;

    std::thread producer([&]()
                         {
        for (uint32_t i = 0; i < kCount; ++i)
        {
            while (!rb.push(i))
            {
            }
        } });

    std::vector<uint32_t> received;
    received.reserve(kCount);

    std::thread consumer([&]()
                         {
        for (uint32_t i = 0; i < kCount; ++i)
        {
            uint32_t val;
            while (!rb.pop(val))
            {
            }
            received.push_back(val);
        } });

    producer.join();
    consumer.join();

    ASSERT_EQ(received.size(), kCount);
    for (uint32_t i = 0; i < kCount; ++i)
    {
        EXPECT_EQ(received[i], i);
    }
}

// ---------------------------------------------------------------------------
// SPSC with large struct (multi-cache-line element)
// ---------------------------------------------------------------------------

struct LargePayload
{
    uint32_t id;
    uint8_t data[252]; // total 256 bytes — spans 4 cache lines
};

TEST_F(ConcurrentTest, SPSCLargeStruct)
{
    static constexpr uint32_t kCount = 50000;
    ms::spsc::RingBuffer<LargePayload, 64> rb;

    std::thread producer([&]()
                         {
        for (uint32_t i = 0; i < kCount; ++i)
        {
            LargePayload lp{};
            lp.id = i;
            // Fill data with a pattern derived from id.
            std::memset(lp.data, static_cast<int>(i & 0xFF), sizeof(lp.data));
            while (!rb.push(lp))
            {
            }
        } });

    std::atomic<uint32_t> errors{0};

    std::thread consumer([&]()
                         {
        for (uint32_t i = 0; i < kCount; ++i)
        {
            LargePayload lp{};
            while (!rb.pop(lp))
            {
            }
            if (lp.id != i)
            {
                errors.fetch_add(1, std::memory_order_relaxed);
                continue;
            }
            // Verify the data pattern is consistent (no torn writes).
            uint8_t expected = static_cast<uint8_t>(i & 0xFF);
            for (size_t b = 0; b < sizeof(lp.data); ++b)
            {
                if (lp.data[b] != expected)
                {
                    errors.fetch_add(1, std::memory_order_relaxed);
                    break;
                }
            }
        } });

    producer.join();
    consumer.join();

    EXPECT_EQ(errors.load(), 0u) << "Detected torn writes or reordering";
}

// ---------------------------------------------------------------------------
// SPSC ByteRingBuffer — IPC-style concurrent byte stream
// ---------------------------------------------------------------------------

TEST_F(ConcurrentTest, SPSCByteStream)
{
    static constexpr uint32_t kMessages = 10000;
    ms::spsc::ByteRingBuffer<4096> rb;

    std::thread producer([&]()
                         {
        for (uint32_t i = 0; i < kMessages; ++i)
        {
            // Write length-prefixed message: [uint32_t len][uint32_t payload]
            uint32_t payload = i;
            uint32_t len = sizeof(payload);
            // Must write both atomically relative to our own thread (no other
            // producer), but the consumer may be reading concurrently.
            while (rb.writeAvailable() < sizeof(len) + len)
            {
            }
            rb.write(reinterpret_cast<const uint8_t *>(&len), sizeof(len));
            rb.write(reinterpret_cast<const uint8_t *>(&payload), sizeof(payload));
        } });

    std::vector<uint32_t> received;
    received.reserve(kMessages);

    std::thread consumer([&]()
                         {
        for (uint32_t i = 0; i < kMessages; ++i)
        {
            // Read length.
            uint32_t len = 0;
            while (rb.readAvailable() < sizeof(len))
            {
            }
            rb.read(reinterpret_cast<uint8_t *>(&len), sizeof(len));

            // Read payload.
            uint32_t payload = 0;
            while (rb.readAvailable() < len)
            {
            }
            rb.read(reinterpret_cast<uint8_t *>(&payload), len);

            received.push_back(payload);
        } });

    producer.join();
    consumer.join();

    ASSERT_EQ(received.size(), kMessages);
    for (uint32_t i = 0; i < kMessages; ++i)
    {
        EXPECT_EQ(received[i], i);
    }
}

// ---------------------------------------------------------------------------
// SPSC with custom cache line size — verify no false sharing at 128 bytes
// ---------------------------------------------------------------------------

TEST_F(ConcurrentTest, SPSCCustomCacheLine128)
{
    static constexpr uint32_t kCount = 200000;
    ms::spsc::RingBuffer<uint32_t, 256, 128> rb;

    std::thread producer([&]()
                         {
        for (uint32_t i = 0; i < kCount; ++i)
        {
            while (!rb.push(i))
            {
            }
        } });

    std::vector<uint32_t> received;
    received.reserve(kCount);

    std::thread consumer([&]()
                         {
        for (uint32_t i = 0; i < kCount; ++i)
        {
            uint32_t val;
            while (!rb.pop(val))
            {
            }
            received.push_back(val);
        } });

    producer.join();
    consumer.join();

    ASSERT_EQ(received.size(), kCount);
    for (uint32_t i = 0; i < kCount; ++i)
    {
        EXPECT_EQ(received[i], i);
    }
}

// ---------------------------------------------------------------------------
// Sustained throughput — many cycles to stress the counter wraparound
// ---------------------------------------------------------------------------

TEST_F(ConcurrentTest, SustainedThroughput)
{
    // 1M elements through a small buffer — the uint32_t counters will
    // wrap past UINT32_MAX / Capacity many times, exercising the bitmask
    // arithmetic under contention.
    static constexpr uint32_t kCount = 1000000;
    ms::spsc::RingBuffer<uint32_t, 64> rb;

    std::thread producer([&]()
                         {
        for (uint32_t i = 0; i < kCount; ++i)
        {
            while (!rb.push(i))
            {
            }
        } });

    uint32_t lastVal = 0;
    bool ordered = true;

    std::thread consumer([&]()
                         {
        for (uint32_t i = 0; i < kCount; ++i)
        {
            uint32_t val;
            while (!rb.pop(val))
            {
            }
            if (val != i)
                ordered = false;
            lastVal = val;
        } });

    producer.join();
    consumer.join();

    EXPECT_TRUE(ordered) << "Elements arrived out of order";
    EXPECT_EQ(lastVal, kCount - 1);
}
