// Tests for RingBuffer with plain-old-data (POD) types.
//
// Uses typed tests (TYPED_TEST_SUITE) to exercise the same logic across
// int, uint8_t, uint32_t, uint64_t, float, double, and char.

#include <RingBuffer.h>

#include <cstdint>
#include <limits>
#include <vector>

#include <gtest/gtest.h>

// ---------------------------------------------------------------------------
// Typed test fixture — runs every test for each POD type.
// ---------------------------------------------------------------------------

template <typename T>
class PodRingBufferTest : public ::testing::Test
{
protected:
    static constexpr uint32_t kCapacity = 16;
    ms::spsc::RingBuffer<T, kCapacity> m_rb;

    // Helper: make a deterministic value for element index i.
    static T makeValue(int i)
    {
        return static_cast<T>(i + 1);
    }
};

using PodTypes = ::testing::Types<
    int, uint8_t, uint32_t, uint64_t, int16_t, float, double, char>;
TYPED_TEST_SUITE(PodRingBufferTest, PodTypes);

// -- Initial state -----------------------------------------------------------

TYPED_TEST(PodRingBufferTest, StartsEmpty)
{
    EXPECT_TRUE(this->m_rb.isEmpty());
    EXPECT_FALSE(this->m_rb.isFull());
    EXPECT_EQ(this->m_rb.readAvailable(), 0u);
    EXPECT_EQ(this->m_rb.writeAvailable(), this->kCapacity);
    EXPECT_EQ(this->m_rb.capacity(), this->kCapacity);
}

// -- Single-element push / pop -----------------------------------------------

TYPED_TEST(PodRingBufferTest, PushPopSingle)
{
    TypeParam in = this->makeValue(42);
    TypeParam out{};

    EXPECT_TRUE(this->m_rb.push(in));
    EXPECT_EQ(this->m_rb.readAvailable(), 1u);
    EXPECT_EQ(this->m_rb.writeAvailable(), this->kCapacity - 1);

    EXPECT_TRUE(this->m_rb.pop(out));
    EXPECT_EQ(out, in);
    EXPECT_TRUE(this->m_rb.isEmpty());
}

// -- Fill to capacity and drain ----------------------------------------------

TYPED_TEST(PodRingBufferTest, FillAndDrain)
{
    for (uint32_t i = 0; i < this->kCapacity; ++i)
    {
        EXPECT_TRUE(this->m_rb.push(this->makeValue(i)));
    }
    EXPECT_TRUE(this->m_rb.isFull());
    EXPECT_EQ(this->m_rb.writeAvailable(), 0u);

    // One more push must fail.
    EXPECT_FALSE(this->m_rb.push(this->makeValue(99)));

    for (uint32_t i = 0; i < this->kCapacity; ++i)
    {
        TypeParam val{};
        EXPECT_TRUE(this->m_rb.pop(val));
        EXPECT_EQ(val, this->makeValue(i));
    }
    EXPECT_TRUE(this->m_rb.isEmpty());
}

// -- Bulk write / read -------------------------------------------------------

TYPED_TEST(PodRingBufferTest, BulkWriteRead)
{
    constexpr uint32_t kCount = 5;
    TypeParam src[kCount];
    for (uint32_t i = 0; i < kCount; ++i)
        src[i] = this->makeValue(i);

    EXPECT_TRUE(this->m_rb.write(src, kCount));
    EXPECT_EQ(this->m_rb.readAvailable(), kCount);

    TypeParam dst[kCount]{};
    EXPECT_TRUE(this->m_rb.read(dst, kCount));
    for (uint32_t i = 0; i < kCount; ++i)
    {
        EXPECT_EQ(dst[i], src[i]);
    }
    EXPECT_TRUE(this->m_rb.isEmpty());
}

// -- Peek does not consume ---------------------------------------------------

TYPED_TEST(PodRingBufferTest, PeekDoesNotConsume)
{
    TypeParam a = this->makeValue(10);
    TypeParam b = this->makeValue(20);
    this->m_rb.push(a);
    this->m_rb.push(b);

    TypeParam peeked[2]{};
    EXPECT_TRUE(this->m_rb.peek(peeked, 2));
    EXPECT_EQ(peeked[0], a);
    EXPECT_EQ(peeked[1], b);

    // Data still there.
    EXPECT_EQ(this->m_rb.readAvailable(), 2u);

    // Pop should return the same values.
    TypeParam v{};
    EXPECT_TRUE(this->m_rb.pop(v));
    EXPECT_EQ(v, a);
}

// -- Skip --------------------------------------------------------------------

TYPED_TEST(PodRingBufferTest, SkipElements)
{
    for (uint32_t i = 0; i < 5; ++i)
        this->m_rb.push(this->makeValue(i));

    EXPECT_TRUE(this->m_rb.skip(3));
    EXPECT_EQ(this->m_rb.readAvailable(), 2u);

    TypeParam val{};
    EXPECT_TRUE(this->m_rb.pop(val));
    EXPECT_EQ(val, this->makeValue(3));
}

// -- Reset -------------------------------------------------------------------

TYPED_TEST(PodRingBufferTest, ResetClearsBuffer)
{
    for (uint32_t i = 0; i < 4; ++i)
        this->m_rb.push(this->makeValue(i));

    this->m_rb.reset();
    EXPECT_TRUE(this->m_rb.isEmpty());
    EXPECT_EQ(this->m_rb.writeAvailable(), this->kCapacity);
}

// -- Wraparound with single elements ----------------------------------------

TYPED_TEST(PodRingBufferTest, WraparoundSingleElements)
{
    // Advance head/tail to near the end.
    for (uint32_t i = 0; i < this->kCapacity - 2; ++i)
    {
        this->m_rb.push(this->makeValue(i));
        TypeParam tmp{};
        this->m_rb.pop(tmp);
    }

    // Now write a full capacity that wraps around.
    for (uint32_t i = 0; i < this->kCapacity; ++i)
    {
        EXPECT_TRUE(this->m_rb.push(this->makeValue(100 + i)));
    }
    EXPECT_TRUE(this->m_rb.isFull());

    for (uint32_t i = 0; i < this->kCapacity; ++i)
    {
        TypeParam val{};
        EXPECT_TRUE(this->m_rb.pop(val));
        EXPECT_EQ(val, this->makeValue(100 + i));
    }
}

// -- Wraparound with bulk write/read ----------------------------------------

TYPED_TEST(PodRingBufferTest, WraparoundBulk)
{
    // Advance past the midpoint.
    constexpr uint32_t kAdvance = 13; // kCapacity is 16
    TypeParam tmp[kAdvance];
    for (uint32_t i = 0; i < kAdvance; ++i)
        tmp[i] = this->makeValue(i);

    this->m_rb.write(tmp, kAdvance);
    TypeParam sink[kAdvance];
    this->m_rb.read(sink, kAdvance);

    // Bulk write that wraps around the end.
    constexpr uint32_t kBulk = 8;
    TypeParam src[kBulk];
    for (uint32_t i = 0; i < kBulk; ++i)
        src[i] = this->makeValue(200 + i);

    EXPECT_TRUE(this->m_rb.write(src, kBulk));

    TypeParam dst[kBulk]{};
    EXPECT_TRUE(this->m_rb.read(dst, kBulk));
    for (uint32_t i = 0; i < kBulk; ++i)
    {
        EXPECT_EQ(dst[i], src[i]);
    }
}

// -- Repeated fill/drain cycles ----------------------------------------------

TYPED_TEST(PodRingBufferTest, RepeatedFillDrainCycles)
{
    for (int cycle = 0; cycle < 10; ++cycle)
    {
        for (uint32_t i = 0; i < this->kCapacity; ++i)
        {
            EXPECT_TRUE(this->m_rb.push(this->makeValue(cycle * 100 + i)));
        }
        EXPECT_TRUE(this->m_rb.isFull());

        for (uint32_t i = 0; i < this->kCapacity; ++i)
        {
            TypeParam val{};
            EXPECT_TRUE(this->m_rb.pop(val));
            EXPECT_EQ(val, this->makeValue(cycle * 100 + i));
        }
        EXPECT_TRUE(this->m_rb.isEmpty());
    }
}

// -- Interleaved push/pop ---------------------------------------------------

TYPED_TEST(PodRingBufferTest, InterleavedPushPop)
{
    // Push 3, pop 1, push 3, pop 1 ... exercises various offsets.
    uint32_t pushed = 0;
    uint32_t popped = 0;

    for (int round = 0; round < 20; ++round)
    {
        for (int i = 0; i < 3 && this->m_rb.writeAvailable() > 0; ++i)
        {
            this->m_rb.push(this->makeValue(pushed));
            ++pushed;
        }
        if (this->m_rb.readAvailable() > 0)
        {
            TypeParam val{};
            this->m_rb.pop(val);
            EXPECT_EQ(val, this->makeValue(popped));
            ++popped;
        }
    }

    // Drain remainder.
    while (this->m_rb.readAvailable() > 0)
    {
        TypeParam val{};
        this->m_rb.pop(val);
        EXPECT_EQ(val, this->makeValue(popped));
        ++popped;
    }
    EXPECT_EQ(pushed, popped);
}

// -- Type-specific: min/max values ------------------------------------------

TYPED_TEST(PodRingBufferTest, MinMaxValues)
{
    TypeParam lo = std::numeric_limits<TypeParam>::lowest();
    TypeParam hi = std::numeric_limits<TypeParam>::max();

    EXPECT_TRUE(this->m_rb.push(lo));
    EXPECT_TRUE(this->m_rb.push(hi));

    TypeParam out{};
    EXPECT_TRUE(this->m_rb.pop(out));
    EXPECT_EQ(out, lo);
    EXPECT_TRUE(this->m_rb.pop(out));
    EXPECT_EQ(out, hi);
}
