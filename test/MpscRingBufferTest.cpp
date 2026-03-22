// MPSC RingBuffer unit + concurrent tests.

#include <mpsc/RingBuffer.h>

#include <gtest/gtest.h>

#include <algorithm>
#include <atomic>
#include <cstring>
#include <thread>
#include <vector>

// ─── Basic single-threaded tests ────────────────────────────────────

class MpscBasicTest : public ::testing::Test {
protected:
    ouroboros::mpsc::RingBuffer<int, 8> m_rb;
};

TEST_F(MpscBasicTest, StartsEmpty) {
    EXPECT_TRUE(m_rb.isEmpty());
    EXPECT_FALSE(m_rb.isFull());
    EXPECT_EQ(m_rb.readAvailable(), 0u);
    EXPECT_EQ(m_rb.writeAvailable(), 8u);
}

TEST_F(MpscBasicTest, Capacity) {
    EXPECT_EQ(m_rb.capacity(), 8u);
}

TEST_F(MpscBasicTest, CacheLineSize) {
    EXPECT_EQ(m_rb.cacheLineSize(), 64u);
}

TEST_F(MpscBasicTest, PushPop) {
    EXPECT_TRUE(m_rb.push(42));
    EXPECT_EQ(m_rb.readAvailable(), 1u);

    int val = 0;
    EXPECT_TRUE(m_rb.pop(val));
    EXPECT_EQ(val, 42);
    EXPECT_TRUE(m_rb.isEmpty());
}

TEST_F(MpscBasicTest, FillAndDrain) {
    for (int i = 0; i < 8; ++i)
        ASSERT_TRUE(m_rb.push(i));

    EXPECT_TRUE(m_rb.isFull());
    EXPECT_FALSE(m_rb.push(99));

    for (int i = 0; i < 8; ++i) {
        int val = -1;
        ASSERT_TRUE(m_rb.pop(val));
        EXPECT_EQ(val, i);
    }
    EXPECT_TRUE(m_rb.isEmpty());
}

TEST_F(MpscBasicTest, PopFromEmpty) {
    int val = 0xDEAD;
    EXPECT_FALSE(m_rb.pop(val));
    EXPECT_EQ(val, 0xDEAD);
}

TEST_F(MpscBasicTest, PushToFull) {
    for (int i = 0; i < 8; ++i)
        ASSERT_TRUE(m_rb.push(i));
    EXPECT_FALSE(m_rb.push(99));
}

TEST_F(MpscBasicTest, Wraparound) {
    for (int cycle = 0; cycle < 20; ++cycle) {
        EXPECT_TRUE(m_rb.push(cycle));
        int val = -1;
        EXPECT_TRUE(m_rb.pop(val));
        EXPECT_EQ(val, cycle);
    }
}

TEST_F(MpscBasicTest, Reset) {
    for (int i = 0; i < 4; ++i)
        ASSERT_TRUE(m_rb.push(i));

    m_rb.reset();
    EXPECT_TRUE(m_rb.isEmpty());
    EXPECT_EQ(m_rb.writeAvailable(), 8u);

    // Must work after reset
    EXPECT_TRUE(m_rb.push(100));
    int val = -1;
    EXPECT_TRUE(m_rb.pop(val));
    EXPECT_EQ(val, 100);
}

TEST_F(MpscBasicTest, FIFOOrder) {
    for (int i = 0; i < 8; ++i)
        ASSERT_TRUE(m_rb.push(i * 10));

    for (int i = 0; i < 8; ++i) {
        int val = -1;
        ASSERT_TRUE(m_rb.pop(val));
        EXPECT_EQ(val, i * 10);
    }
}

// ─── Capacity-1 edge case ───────────────────────────────────────────

TEST(MpscMinCapacity, PushPopCycle) {
    ouroboros::mpsc::RingBuffer<int, 1> rb;
    EXPECT_EQ(rb.capacity(), 1u);

    for (int i = 0; i < 100; ++i) {
        EXPECT_TRUE(rb.push(i));
        EXPECT_TRUE(rb.isFull());
        int val = -1;
        EXPECT_TRUE(rb.pop(val));
        EXPECT_EQ(val, i);
    }
}

// ─── Custom cache line size ─────────────────────────────────────────

TEST(MpscCacheLine, Size128) {
    ouroboros::mpsc::RingBuffer<int, 8, 128> rb;
    EXPECT_EQ(rb.cacheLineSize(), 128u);

    EXPECT_TRUE(rb.push(1));
    int val = -1;
    EXPECT_TRUE(rb.pop(val));
    EXPECT_EQ(val, 1);
}

// ─── Struct type ────────────────────────────────────────────────────

namespace {
struct Payload {
    uint32_t id;
    uint32_t checksum;
};
}

TEST(MpscStruct, PushPopStruct) {
    ouroboros::mpsc::RingBuffer<Payload, 16> rb;

    Payload p{42, 42 * 7};
    EXPECT_TRUE(rb.push(p));

    Payload out{};
    EXPECT_TRUE(rb.pop(out));
    EXPECT_EQ(out.id, 42u);
    EXPECT_EQ(out.checksum, 42u * 7);
}

// ─── Concurrent tests ──────────────────────────────────────────────

TEST(MpscConcurrent, TwoProducers) {
    ouroboros::mpsc::RingBuffer<uint32_t, 1024> rb;
    constexpr uint32_t kPerProducer = 100000;

    auto producer = [&](uint32_t base) {
        for (uint32_t i = 0; i < kPerProducer; ++i) {
            while (!rb.push(base + i)) {}
        }
    };

    std::vector<uint32_t> received;
    received.reserve(kPerProducer * 2);
    std::atomic<bool> done{false};

    std::thread p1(producer, 0);
    std::thread p2(producer, kPerProducer);

    std::thread consumer([&] {
        uint32_t val;
        while (!done.load(std::memory_order_acquire) || !rb.isEmpty()) {
            if (rb.pop(val))
                received.push_back(val);
        }
    });

    p1.join();
    p2.join();
    done.store(true, std::memory_order_release);
    consumer.join();

    ASSERT_EQ(received.size(), kPerProducer * 2);

    // Every value appears exactly once
    std::sort(received.begin(), received.end());
    for (uint32_t i = 0; i < kPerProducer * 2; ++i) {
        EXPECT_EQ(received[i], i) << "Missing or duplicate at " << i;
    }
}

TEST(MpscConcurrent, FourProducers) {
    ouroboros::mpsc::RingBuffer<uint32_t, 512> rb;
    constexpr uint32_t kPerProducer = 50000;
    constexpr uint32_t kNumProducers = 4;

    auto producer = [&](uint32_t base) {
        for (uint32_t i = 0; i < kPerProducer; ++i) {
            while (!rb.push(base + i)) {}
        }
    };

    std::vector<uint32_t> received;
    received.reserve(kPerProducer * kNumProducers);
    std::atomic<bool> done{false};

    std::vector<std::thread> producers;
    for (uint32_t p = 0; p < kNumProducers; ++p)
        producers.emplace_back(producer, p * kPerProducer);

    std::thread consumer([&] {
        uint32_t val;
        while (!done.load(std::memory_order_acquire) || !rb.isEmpty()) {
            if (rb.pop(val))
                received.push_back(val);
        }
    });

    for (auto &t : producers) t.join();
    done.store(true, std::memory_order_release);
    consumer.join();

    ASSERT_EQ(received.size(), kPerProducer * kNumProducers);

    std::sort(received.begin(), received.end());
    for (uint32_t i = 0; i < kPerProducer * kNumProducers; ++i) {
        EXPECT_EQ(received[i], i) << "Mismatch at " << i;
    }
}

TEST(MpscConcurrent, EightProducersStruct) {
    ouroboros::mpsc::RingBuffer<Payload, 256> rb;
    constexpr uint32_t kPerProducer = 25000;
    constexpr uint32_t kNumProducers = 8;

    auto producer = [&](uint32_t base) {
        for (uint32_t i = 0; i < kPerProducer; ++i) {
            Payload p{base + i, (base + i) * 7};
            while (!rb.push(p)) {}
        }
    };

    std::vector<Payload> received;
    received.reserve(kPerProducer * kNumProducers);
    std::atomic<bool> done{false};

    std::vector<std::thread> producers;
    for (uint32_t p = 0; p < kNumProducers; ++p)
        producers.emplace_back(producer, p * kPerProducer);

    std::thread consumer([&] {
        Payload val{};
        while (!done.load(std::memory_order_acquire) || !rb.isEmpty()) {
            if (rb.pop(val))
                received.push_back(val);
        }
    });

    for (auto &t : producers) t.join();
    done.store(true, std::memory_order_release);
    consumer.join();

    ASSERT_EQ(received.size(), kPerProducer * kNumProducers);

    // Verify all payloads intact
    std::sort(received.begin(), received.end(),
              [](const Payload &a, const Payload &b) { return a.id < b.id; });
    for (uint32_t i = 0; i < kPerProducer * kNumProducers; ++i) {
        EXPECT_EQ(received[i].id, i);
        EXPECT_EQ(received[i].checksum, i * 7);
    }
}

TEST(MpscConcurrent, HighContention) {
    ouroboros::mpsc::RingBuffer<uint32_t, 4> rb;
    constexpr uint32_t kPerProducer = 10000;
    constexpr uint32_t kNumProducers = 4;

    auto producer = [&](uint32_t base) {
        for (uint32_t i = 0; i < kPerProducer; ++i) {
            while (!rb.push(base + i)) {}
        }
    };

    std::vector<uint32_t> received;
    received.reserve(kPerProducer * kNumProducers);
    std::atomic<bool> done{false};

    std::vector<std::thread> producers;
    for (uint32_t p = 0; p < kNumProducers; ++p)
        producers.emplace_back(producer, p * kPerProducer);

    std::thread consumer([&] {
        uint32_t val;
        while (!done.load(std::memory_order_acquire) || !rb.isEmpty()) {
            if (rb.pop(val))
                received.push_back(val);
        }
    });

    for (auto &t : producers) t.join();
    done.store(true, std::memory_order_release);
    consumer.join();

    ASSERT_EQ(received.size(), kPerProducer * kNumProducers);
    std::sort(received.begin(), received.end());
    for (uint32_t i = 0; i < kPerProducer * kNumProducers; ++i) {
        EXPECT_EQ(received[i], i);
    }
}

TEST(MpscConcurrent, SustainedThroughput) {
    ouroboros::mpsc::RingBuffer<uint64_t, 64> rb;
    constexpr uint64_t kTotal = 1000000;

    std::thread producer([&] {
        for (uint64_t i = 0; i < kTotal; ++i) {
            while (!rb.push(i)) {}
        }
    });

    std::vector<uint64_t> received;
    received.reserve(kTotal);

    std::thread consumer([&] {
        uint64_t val;
        while (received.size() < kTotal) {
            if (rb.pop(val))
                received.push_back(val);
        }
    });

    producer.join();
    consumer.join();

    ASSERT_EQ(received.size(), kTotal);
    for (uint64_t i = 0; i < kTotal; ++i) {
        EXPECT_EQ(received[i], i);
    }
}
