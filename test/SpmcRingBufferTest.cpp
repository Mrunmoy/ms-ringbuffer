// SPMC RingBuffer unit + concurrent tests.

#include <spmc/RingBuffer.h>

#include <gtest/gtest.h>

#include <algorithm>
#include <atomic>
#include <cstring>
#include <thread>
#include <vector>

// ─── Basic single-threaded tests ────────────────────────────────────

class SpmcBasicTest : public ::testing::Test {
protected:
    ouroboros::spmc::RingBuffer<int, 8> m_rb;
};

TEST_F(SpmcBasicTest, StartsEmpty) {
    EXPECT_TRUE(m_rb.isEmpty());
    EXPECT_FALSE(m_rb.isFull());
    EXPECT_EQ(m_rb.readAvailable(), 0u);
    EXPECT_EQ(m_rb.writeAvailable(), 8u);
}

TEST_F(SpmcBasicTest, Capacity) {
    EXPECT_EQ(m_rb.capacity(), 8u);
}

TEST_F(SpmcBasicTest, CacheLineSize) {
    EXPECT_EQ(m_rb.cacheLineSize(), 64u);
}

TEST_F(SpmcBasicTest, PushPop) {
    EXPECT_TRUE(m_rb.push(42));
    EXPECT_EQ(m_rb.readAvailable(), 1u);

    int val = 0;
    EXPECT_TRUE(m_rb.pop(val));
    EXPECT_EQ(val, 42);
    EXPECT_TRUE(m_rb.isEmpty());
}

TEST_F(SpmcBasicTest, FillAndDrain) {
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

TEST_F(SpmcBasicTest, PopFromEmpty) {
    int val = 0xDEAD;
    EXPECT_FALSE(m_rb.pop(val));
    EXPECT_EQ(val, 0xDEAD);
}

TEST_F(SpmcBasicTest, PushToFull) {
    for (int i = 0; i < 8; ++i)
        ASSERT_TRUE(m_rb.push(i));
    EXPECT_FALSE(m_rb.push(99));
}

TEST_F(SpmcBasicTest, Wraparound) {
    for (int cycle = 0; cycle < 20; ++cycle) {
        EXPECT_TRUE(m_rb.push(cycle));
        int val = -1;
        EXPECT_TRUE(m_rb.pop(val));
        EXPECT_EQ(val, cycle);
    }
}

TEST_F(SpmcBasicTest, Reset) {
    for (int i = 0; i < 4; ++i)
        ASSERT_TRUE(m_rb.push(i));

    m_rb.reset();
    EXPECT_TRUE(m_rb.isEmpty());
    EXPECT_EQ(m_rb.writeAvailable(), 8u);

    EXPECT_TRUE(m_rb.push(100));
    int val = -1;
    EXPECT_TRUE(m_rb.pop(val));
    EXPECT_EQ(val, 100);
}

TEST_F(SpmcBasicTest, FIFOOrder) {
    for (int i = 0; i < 8; ++i)
        ASSERT_TRUE(m_rb.push(i * 10));

    for (int i = 0; i < 8; ++i) {
        int val = -1;
        ASSERT_TRUE(m_rb.pop(val));
        EXPECT_EQ(val, i * 10);
    }
}

// ─── Capacity-1 edge case ───────────────────────────────────────────

TEST(SpmcMinCapacity, PushPopCycle) {
    ouroboros::spmc::RingBuffer<int, 1> rb;
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

TEST(SpmcCacheLine, Size128) {
    ouroboros::spmc::RingBuffer<int, 8, 128> rb;
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

TEST(SpmcStruct, PushPopStruct) {
    ouroboros::spmc::RingBuffer<Payload, 16> rb;

    Payload p{42, 42 * 7};
    EXPECT_TRUE(rb.push(p));

    Payload out{};
    EXPECT_TRUE(rb.pop(out));
    EXPECT_EQ(out.id, 42u);
    EXPECT_EQ(out.checksum, 42u * 7);
}

// ─── Concurrent tests ──────────────────────────────────────────────

TEST(SpmcConcurrent, TwoConsumers) {
    ouroboros::spmc::RingBuffer<uint32_t, 1024> rb;
    constexpr uint32_t kTotal = 200000;
    constexpr uint32_t kNumConsumers = 2;

    std::atomic<bool> done{false};

    std::thread producer([&] {
        for (uint32_t i = 0; i < kTotal; ++i) {
            while (!rb.push(i)) {}
        }
        done.store(true, std::memory_order_release);
    });

    std::vector<std::vector<uint32_t>> per_consumer(kNumConsumers);
    std::vector<std::thread> consumers;

    for (uint32_t c = 0; c < kNumConsumers; ++c) {
        per_consumer[c].reserve(kTotal);
        consumers.emplace_back([&, c] {
            uint32_t val;
            while (!done.load(std::memory_order_acquire) || !rb.isEmpty()) {
                if (rb.pop(val))
                    per_consumer[c].push_back(val);
            }
        });
    }

    producer.join();
    consumers[0].join();
    consumers[1].join();

    // Merge and verify: every value appears exactly once
    std::vector<uint32_t> all;
    for (auto &v : per_consumer)
        all.insert(all.end(), v.begin(), v.end());

    ASSERT_EQ(all.size(), kTotal);
    std::sort(all.begin(), all.end());
    for (uint32_t i = 0; i < kTotal; ++i) {
        EXPECT_EQ(all[i], i) << "Missing or duplicate at " << i;
    }
}

TEST(SpmcConcurrent, FourConsumers) {
    ouroboros::spmc::RingBuffer<uint32_t, 512> rb;
    constexpr uint32_t kTotal = 200000;
    constexpr uint32_t kNumConsumers = 4;

    std::atomic<bool> done{false};

    std::thread producer([&] {
        for (uint32_t i = 0; i < kTotal; ++i) {
            while (!rb.push(i)) {}
        }
        done.store(true, std::memory_order_release);
    });

    std::vector<std::vector<uint32_t>> per_consumer(kNumConsumers);
    std::vector<std::thread> consumers;

    for (uint32_t c = 0; c < kNumConsumers; ++c) {
        per_consumer[c].reserve(kTotal);
        consumers.emplace_back([&, c] {
            uint32_t val;
            while (!done.load(std::memory_order_acquire) || !rb.isEmpty()) {
                if (rb.pop(val))
                    per_consumer[c].push_back(val);
            }
        });
    }

    producer.join();
    for (auto &t : consumers) t.join();

    std::vector<uint32_t> all;
    for (auto &v : per_consumer)
        all.insert(all.end(), v.begin(), v.end());

    ASSERT_EQ(all.size(), kTotal);
    std::sort(all.begin(), all.end());
    for (uint32_t i = 0; i < kTotal; ++i) {
        EXPECT_EQ(all[i], i) << "Mismatch at " << i;
    }
}

TEST(SpmcConcurrent, EightConsumersStruct) {
    ouroboros::spmc::RingBuffer<Payload, 256> rb;
    constexpr uint32_t kTotal = 200000;
    constexpr uint32_t kNumConsumers = 8;

    std::atomic<bool> done{false};

    std::thread producer([&] {
        for (uint32_t i = 0; i < kTotal; ++i) {
            Payload p{i, i * 7};
            while (!rb.push(p)) {}
        }
        done.store(true, std::memory_order_release);
    });

    std::vector<std::vector<Payload>> per_consumer(kNumConsumers);
    std::vector<std::thread> consumers;

    for (uint32_t c = 0; c < kNumConsumers; ++c) {
        per_consumer[c].reserve(kTotal);
        consumers.emplace_back([&, c] {
            Payload val{};
            while (!done.load(std::memory_order_acquire) || !rb.isEmpty()) {
                if (rb.pop(val))
                    per_consumer[c].push_back(val);
            }
        });
    }

    producer.join();
    for (auto &t : consumers) t.join();

    std::vector<Payload> all;
    for (auto &v : per_consumer)
        all.insert(all.end(), v.begin(), v.end());

    ASSERT_EQ(all.size(), kTotal);
    std::sort(all.begin(), all.end(),
              [](const Payload &a, const Payload &b) { return a.id < b.id; });
    for (uint32_t i = 0; i < kTotal; ++i) {
        EXPECT_EQ(all[i].id, i);
        EXPECT_EQ(all[i].checksum, i * 7);
    }
}

TEST(SpmcConcurrent, HighContention) {
    ouroboros::spmc::RingBuffer<uint32_t, 4> rb;
    constexpr uint32_t kTotal = 40000;
    constexpr uint32_t kNumConsumers = 4;

    std::atomic<bool> done{false};

    std::thread producer([&] {
        for (uint32_t i = 0; i < kTotal; ++i) {
            while (!rb.push(i)) {}
        }
        done.store(true, std::memory_order_release);
    });

    std::vector<std::vector<uint32_t>> per_consumer(kNumConsumers);
    std::vector<std::thread> consumers;

    for (uint32_t c = 0; c < kNumConsumers; ++c) {
        per_consumer[c].reserve(kTotal);
        consumers.emplace_back([&, c] {
            uint32_t val;
            while (!done.load(std::memory_order_acquire) || !rb.isEmpty()) {
                if (rb.pop(val))
                    per_consumer[c].push_back(val);
            }
        });
    }

    producer.join();
    for (auto &t : consumers) t.join();

    std::vector<uint32_t> all;
    for (auto &v : per_consumer)
        all.insert(all.end(), v.begin(), v.end());

    ASSERT_EQ(all.size(), kTotal);
    std::sort(all.begin(), all.end());
    for (uint32_t i = 0; i < kTotal; ++i) {
        EXPECT_EQ(all[i], i);
    }
}

TEST(SpmcConcurrent, SustainedThroughput) {
    ouroboros::spmc::RingBuffer<uint64_t, 64> rb;
    constexpr uint64_t kTotal = 1000000;
    constexpr uint32_t kNumConsumers = 2;

    std::atomic<bool> done{false};

    std::thread producer([&] {
        for (uint64_t i = 0; i < kTotal; ++i) {
            while (!rb.push(i)) {}
        }
        done.store(true, std::memory_order_release);
    });

    std::vector<std::vector<uint64_t>> per_consumer(kNumConsumers);
    std::vector<std::thread> consumers;

    for (uint32_t c = 0; c < kNumConsumers; ++c) {
        per_consumer[c].reserve(kTotal);
        consumers.emplace_back([&, c] {
            uint64_t val;
            while (!done.load(std::memory_order_acquire) || !rb.isEmpty()) {
                if (rb.pop(val))
                    per_consumer[c].push_back(val);
            }
        });
    }

    producer.join();
    for (auto &t : consumers) t.join();

    std::vector<uint64_t> all;
    for (auto &v : per_consumer)
        all.insert(all.end(), v.begin(), v.end());

    ASSERT_EQ(all.size(), kTotal);
    std::sort(all.begin(), all.end());
    for (uint64_t i = 0; i < kTotal; ++i) {
        EXPECT_EQ(all[i], i);
    }
}
