// Boundary and edge-case tests for RingBuffer.
//
// Covers: minimum capacity, empty/full operations, exact-capacity writes,
// zero-count operations, counter wraparound, concurrent SPSC stress, and
// custom cache line sizes.

#include <spsc/RingBuffer.h>

#include <cstdint>
#include <thread>
#include <vector>

#include <gtest/gtest.h>

// ---------------------------------------------------------------------------
// Minimum capacity (1 element)
// ---------------------------------------------------------------------------

class MinCapacityTest : public ::testing::Test
{
protected:
    ms::spsc::RingBuffer<int, 1> m_rb;
};

TEST_F(MinCapacityTest, Capacity)
{
    EXPECT_EQ(m_rb.capacity(), 1u);
    EXPECT_EQ(m_rb.writeAvailable(), 1u);
    EXPECT_EQ(m_rb.readAvailable(), 0u);
}

TEST_F(MinCapacityTest, PushPopOnce)
{
    EXPECT_TRUE(m_rb.push(42));
    EXPECT_TRUE(m_rb.isFull());
    EXPECT_FALSE(m_rb.push(99)); // Full.

    int val = 0;
    EXPECT_TRUE(m_rb.pop(val));
    EXPECT_EQ(val, 42);
    EXPECT_TRUE(m_rb.isEmpty());
}

TEST_F(MinCapacityTest, RepeatedPushPop)
{
    for (int i = 0; i < 100; ++i)
    {
        EXPECT_TRUE(m_rb.push(i));
        int val = -1;
        EXPECT_TRUE(m_rb.pop(val));
        EXPECT_EQ(val, i);
    }
}

// ---------------------------------------------------------------------------
// Small capacity (2 elements)
// ---------------------------------------------------------------------------

class SmallCapacityTest : public ::testing::Test
{
protected:
    ms::spsc::RingBuffer<uint64_t, 2> m_rb;
};

TEST_F(SmallCapacityTest, FillAndDrain)
{
    EXPECT_TRUE(m_rb.push(111));
    EXPECT_TRUE(m_rb.push(222));
    EXPECT_TRUE(m_rb.isFull());
    EXPECT_FALSE(m_rb.push(333));

    uint64_t v = 0;
    EXPECT_TRUE(m_rb.pop(v));
    EXPECT_EQ(v, 111u);
    EXPECT_TRUE(m_rb.pop(v));
    EXPECT_EQ(v, 222u);
    EXPECT_TRUE(m_rb.isEmpty());
}

TEST_F(SmallCapacityTest, BulkExactFit)
{
    uint64_t src[2] = {10, 20};
    EXPECT_TRUE(m_rb.write(src, 2));
    EXPECT_TRUE(m_rb.isFull());

    uint64_t dst[2]{};
    EXPECT_TRUE(m_rb.read(dst, 2));
    EXPECT_EQ(dst[0], 10u);
    EXPECT_EQ(dst[1], 20u);
}

// ---------------------------------------------------------------------------
// Empty buffer operations (underflow)
// ---------------------------------------------------------------------------

class EmptyBufferTest : public ::testing::Test
{
protected:
    ms::spsc::RingBuffer<int, 8> m_rb;
};

TEST_F(EmptyBufferTest, PopFromEmpty)
{
    int val = 42;
    EXPECT_FALSE(m_rb.pop(val));
    EXPECT_EQ(val, 42); // Unchanged.
}

TEST_F(EmptyBufferTest, ReadFromEmpty)
{
    int buf[4]{};
    EXPECT_FALSE(m_rb.read(buf, 4));
    EXPECT_FALSE(m_rb.read(buf, 1));
}

TEST_F(EmptyBufferTest, PeekFromEmpty)
{
    int buf[1]{};
    EXPECT_FALSE(m_rb.peek(buf, 1));
}

TEST_F(EmptyBufferTest, SkipFromEmpty)
{
    EXPECT_FALSE(m_rb.skip(1));
}

TEST_F(EmptyBufferTest, ReadMoreThanAvailable)
{
    m_rb.push(1);
    m_rb.push(2);
    int buf[4]{};
    EXPECT_FALSE(m_rb.read(buf, 4)); // Only 2 available.
    EXPECT_EQ(m_rb.readAvailable(), 2u); // Unchanged.
}

// ---------------------------------------------------------------------------
// Full buffer operations (overflow)
// ---------------------------------------------------------------------------

class FullBufferTest : public ::testing::Test
{
protected:
    ms::spsc::RingBuffer<int, 4> m_rb;

    void SetUp() override
    {
        for (int i = 0; i < 4; ++i)
            m_rb.push(i);
    }
};

TEST_F(FullBufferTest, PushToFull)
{
    EXPECT_TRUE(m_rb.isFull());
    EXPECT_FALSE(m_rb.push(999));
    EXPECT_EQ(m_rb.writeAvailable(), 0u);
}

TEST_F(FullBufferTest, WriteToFull)
{
    int src[1] = {999};
    EXPECT_FALSE(m_rb.write(src, 1));
}

TEST_F(FullBufferTest, WriteExceedingCapacity)
{
    // Even after draining, writing more than capacity fails.
    int sink[4];
    m_rb.read(sink, 4);

    int src[5] = {1, 2, 3, 4, 5};
    EXPECT_FALSE(m_rb.write(src, 5));
    EXPECT_TRUE(m_rb.isEmpty()); // Buffer state unchanged.
}

TEST_F(FullBufferTest, PopOneThenPushOne)
{
    int val = 0;
    EXPECT_TRUE(m_rb.pop(val));
    EXPECT_EQ(val, 0);

    EXPECT_TRUE(m_rb.push(999));
    EXPECT_TRUE(m_rb.isFull());

    // Drain and verify order: 1, 2, 3, 999.
    int expected[] = {1, 2, 3, 999};
    for (int i = 0; i < 4; ++i)
    {
        int v = -1;
        EXPECT_TRUE(m_rb.pop(v));
        EXPECT_EQ(v, expected[i]);
    }
}

// ---------------------------------------------------------------------------
// Exact capacity operations
// ---------------------------------------------------------------------------

TEST(ExactCapacity, WriteExactlyCapacity)
{
    ms::spsc::RingBuffer<int, 8> rb;
    int src[8];
    for (int i = 0; i < 8; ++i)
        src[i] = i * 100;

    EXPECT_TRUE(rb.write(src, 8));
    EXPECT_TRUE(rb.isFull());

    int dst[8]{};
    EXPECT_TRUE(rb.read(dst, 8));
    for (int i = 0; i < 8; ++i)
    {
        EXPECT_EQ(dst[i], i * 100);
    }
    EXPECT_TRUE(rb.isEmpty());
}

TEST(ExactCapacity, PeekExactlyCapacity)
{
    ms::spsc::RingBuffer<int, 4> rb;
    int src[4] = {10, 20, 30, 40};
    rb.write(src, 4);

    int peeked[4]{};
    EXPECT_TRUE(rb.peek(peeked, 4));
    for (int i = 0; i < 4; ++i)
    {
        EXPECT_EQ(peeked[i], src[i]);
    }
    EXPECT_EQ(rb.readAvailable(), 4u); // Not consumed.
}

TEST(ExactCapacity, SkipExactlyCapacity)
{
    ms::spsc::RingBuffer<int, 4> rb;
    int src[4] = {1, 2, 3, 4};
    rb.write(src, 4);

    EXPECT_TRUE(rb.skip(4));
    EXPECT_TRUE(rb.isEmpty());
}

// ---------------------------------------------------------------------------
// Zero-count operations (no-ops, should succeed)
// ---------------------------------------------------------------------------

TEST(ZeroCount, WriteZero)
{
    ms::spsc::RingBuffer<int, 4> rb;
    int src[1] = {42};
    EXPECT_TRUE(rb.write(src, 0));
    EXPECT_TRUE(rb.isEmpty());
}

TEST(ZeroCount, ReadZero)
{
    ms::spsc::RingBuffer<int, 4> rb;
    rb.push(1);
    int dst[1]{};
    EXPECT_TRUE(rb.read(dst, 0));
    EXPECT_EQ(rb.readAvailable(), 1u); // Unchanged.
}

TEST(ZeroCount, PeekZero)
{
    ms::spsc::RingBuffer<int, 4> rb;
    int dst[1]{};
    EXPECT_TRUE(rb.peek(dst, 0));
}

TEST(ZeroCount, SkipZero)
{
    ms::spsc::RingBuffer<int, 4> rb;
    EXPECT_TRUE(rb.skip(0));
}

// ---------------------------------------------------------------------------
// Wraparound at exact boundary
// ---------------------------------------------------------------------------

TEST(Wraparound, ExactBoundaryBulkWrite)
{
    ms::spsc::RingBuffer<int, 8> rb;

    // Advance head/tail to exactly position 8 (= capacity, wraps to 0).
    int tmp[8];
    for (int i = 0; i < 8; ++i)
        tmp[i] = i;
    rb.write(tmp, 8);
    int sink[8];
    rb.read(sink, 8);

    // Head and tail are now at offset 8, which masks to 0.
    // Write should work cleanly.
    int src[8];
    for (int i = 0; i < 8; ++i)
        src[i] = 100 + i;
    EXPECT_TRUE(rb.write(src, 8));

    int dst[8]{};
    EXPECT_TRUE(rb.read(dst, 8));
    for (int i = 0; i < 8; ++i)
    {
        EXPECT_EQ(dst[i], 100 + i);
    }
}

TEST(Wraparound, SingleElementAtEveryOffset)
{
    ms::spsc::RingBuffer<int, 4> rb;

    // Push and pop one element at each internal offset.
    for (int i = 0; i < 20; ++i)
    {
        EXPECT_TRUE(rb.push(i));
        int val = -1;
        EXPECT_TRUE(rb.pop(val));
        EXPECT_EQ(val, i);
    }
}

// ---------------------------------------------------------------------------
// Reset after partial use
// ---------------------------------------------------------------------------

TEST(ResetBehavior, ResetMidStream)
{
    ms::spsc::RingBuffer<int, 8> rb;
    rb.push(1);
    rb.push(2);
    rb.push(3);

    int tmp;
    rb.pop(tmp); // Read one.

    rb.reset();
    EXPECT_TRUE(rb.isEmpty());
    EXPECT_EQ(rb.writeAvailable(), 8u);

    // Can write full capacity after reset.
    for (int i = 0; i < 8; ++i)
    {
        EXPECT_TRUE(rb.push(i + 100));
    }
    EXPECT_TRUE(rb.isFull());

    for (int i = 0; i < 8; ++i)
    {
        int val = -1;
        rb.pop(val);
        EXPECT_EQ(val, i + 100);
    }
}

// ---------------------------------------------------------------------------
// Custom cache line size
// ---------------------------------------------------------------------------

TEST(CacheLineSize, DefaultIs64)
{
    ms::spsc::RingBuffer<int, 8> rb;
    EXPECT_EQ(rb.cacheLineSize(), 64u);
}

TEST(CacheLineSize, CustomSize128)
{
    ms::spsc::RingBuffer<int, 8, 128> rb;
    EXPECT_EQ(rb.cacheLineSize(), 128u);

    // Functional test — should work identically.
    rb.push(42);
    int val = 0;
    rb.pop(val);
    EXPECT_EQ(val, 42);
}

TEST(CacheLineSize, ControlBlockAlignment)
{
    // The ControlBlock should be aligned to CacheLineSize.
    using RB64 = ms::spsc::RingBuffer<int, 8, 64>;
    static_assert(alignof(RB64::ControlBlock) == 64,
                  "ControlBlock must be aligned to CacheLineSize=64");

    using RB128 = ms::spsc::RingBuffer<int, 8, 128>;
    static_assert(alignof(RB128::ControlBlock) == 128,
                  "ControlBlock must be aligned to CacheLineSize=128");
}

// ---------------------------------------------------------------------------
// Version
// ---------------------------------------------------------------------------

TEST(Version, ConstexprsAccessible)
{
    EXPECT_EQ(ms::spsc::Version::major, 1);
    EXPECT_EQ(ms::spsc::Version::minor, 0);
    EXPECT_EQ(ms::spsc::Version::patch, 0);
    EXPECT_EQ(ms::spsc::Version::packed, 0x010000u);
}

// ---------------------------------------------------------------------------
// Concurrent SPSC stress test
// ---------------------------------------------------------------------------

TEST(Concurrent, SPSCStress)
{
    static constexpr uint32_t kCount = 200000;
    ms::spsc::RingBuffer<uint32_t, 1024> rb;

    std::thread producer([&]() {
        for (uint32_t i = 0; i < kCount; ++i)
        {
            while (!rb.push(i))
            {
                // spin — wait for consumer to drain
            }
        }
    });

    std::vector<uint32_t> received;
    received.reserve(kCount);

    std::thread consumer([&]() {
        uint32_t val;
        for (uint32_t i = 0; i < kCount; ++i)
        {
            while (!rb.pop(val))
            {
                // spin — wait for producer to write
            }
            received.push_back(val);
        }
    });

    producer.join();
    consumer.join();

    ASSERT_EQ(received.size(), kCount);
    for (uint32_t i = 0; i < kCount; ++i)
    {
        EXPECT_EQ(received[i], i) << "Mismatch at index " << i;
    }
}

TEST(Concurrent, SPSCStressWithStructs)
{
    struct Event
    {
        uint32_t id;
        uint32_t payload;
    };

    static constexpr uint32_t kCount = 100000;
    ms::spsc::RingBuffer<Event, 512> rb;

    std::thread producer([&]() {
        for (uint32_t i = 0; i < kCount; ++i)
        {
            Event e{i, i * 10};
            while (!rb.push(e))
            {
            }
        }
    });

    std::vector<Event> received;
    received.reserve(kCount);

    std::thread consumer([&]() {
        for (uint32_t i = 0; i < kCount; ++i)
        {
            Event e{};
            while (!rb.pop(e))
            {
            }
            received.push_back(e);
        }
    });

    producer.join();
    consumer.join();

    ASSERT_EQ(received.size(), kCount);
    for (uint32_t i = 0; i < kCount; ++i)
    {
        EXPECT_EQ(received[i].id, i);
        EXPECT_EQ(received[i].payload, i * 10);
    }
}

// ---------------------------------------------------------------------------
// Static assertions — compile-time safety
// ---------------------------------------------------------------------------

// These verify that invalid instantiations produce compile errors.
// (Uncomment one at a time to verify the static_assert fires.)
//
// ms::spsc::RingBuffer<int, 3>  rb_bad_size;   // Not a power of 2.
// ms::spsc::RingBuffer<int, 0>  rb_zero;       // Zero capacity.
//
// struct NonTriviallyCopyable {
//     std::string s;
// };
// ms::spsc::RingBuffer<NonTriviallyCopyable, 4> rb_bad_type;  // Not trivially copyable.
