// Tests for RingBuffer with user-defined struct types.
//
// Demonstrates that the ring buffer works with any trivially copyable struct,
// including structs with arrays, nested structs, and packed layouts.

#include <RingBuffer.h>

#include <cstdint>
#include <cstring>

#include <gtest/gtest.h>

// ---------------------------------------------------------------------------
// Test structs
// ---------------------------------------------------------------------------

struct Point2D
{
    float x;
    float y;
};

inline bool operator==(const Point2D &a, const Point2D &b)
{
    return a.x == b.x && a.y == b.y;
}

struct SensorReading
{
    uint32_t sensorId;
    float value;
    uint64_t timestamp;
};

inline bool operator==(const SensorReading &a, const SensorReading &b)
{
    return a.sensorId == b.sensorId &&
           a.value == b.value &&
           a.timestamp == b.timestamp;
}

struct PacketHeader
{
    uint8_t type;
    uint8_t flags;
    uint16_t length;
    uint32_t sequenceNumber;
};

inline bool operator==(const PacketHeader &a, const PacketHeader &b)
{
    return a.type == b.type &&
           a.flags == b.flags &&
           a.length == b.length &&
           a.sequenceNumber == b.sequenceNumber;
}

struct FixedString
{
    char data[32];
    uint32_t length;
};

inline bool operator==(const FixedString &a, const FixedString &b)
{
    return a.length == b.length && std::memcmp(a.data, b.data, a.length) == 0;
}

struct NestedStruct
{
    Point2D position;
    Point2D velocity;
    uint64_t id;
};

inline bool operator==(const NestedStruct &a, const NestedStruct &b)
{
    return a.position == b.position &&
           a.velocity == b.velocity &&
           a.id == b.id;
}

// ---------------------------------------------------------------------------
// Fixture for Point2D
// ---------------------------------------------------------------------------

class Point2DRingBufferTest : public ::testing::Test
{
protected:
    ms::spsc::RingBuffer<Point2D, 8> m_rb;
};

TEST_F(Point2DRingBufferTest, PushPop)
{
    Point2D in{1.5f, -2.5f};
    Point2D out{};

    EXPECT_TRUE(m_rb.push(in));
    EXPECT_TRUE(m_rb.pop(out));
    EXPECT_EQ(out, in);
}

TEST_F(Point2DRingBufferTest, BulkWriteRead)
{
    Point2D src[] = {{0, 0}, {1, 1}, {2, 4}, {3, 9}};
    EXPECT_TRUE(m_rb.write(src, 4));

    Point2D dst[4]{};
    EXPECT_TRUE(m_rb.read(dst, 4));
    for (int i = 0; i < 4; ++i)
    {
        EXPECT_EQ(dst[i], src[i]);
    }
}

TEST_F(Point2DRingBufferTest, FillDrainAndRefill)
{
    for (int cycle = 0; cycle < 5; ++cycle)
    {
        for (int i = 0; i < 8; ++i)
        {
            Point2D p{static_cast<float>(cycle), static_cast<float>(i)};
            EXPECT_TRUE(m_rb.push(p));
        }
        EXPECT_TRUE(m_rb.isFull());

        for (int i = 0; i < 8; ++i)
        {
            Point2D out{};
            EXPECT_TRUE(m_rb.pop(out));
            EXPECT_FLOAT_EQ(out.x, static_cast<float>(cycle));
            EXPECT_FLOAT_EQ(out.y, static_cast<float>(i));
        }
    }
}

// ---------------------------------------------------------------------------
// Fixture for SensorReading
// ---------------------------------------------------------------------------

class SensorRingBufferTest : public ::testing::Test
{
protected:
    ms::spsc::RingBuffer<SensorReading, 16> m_rb;

    SensorReading makeSample(uint32_t id, float val, uint64_t ts)
    {
        return {id, val, ts};
    }
};

TEST_F(SensorRingBufferTest, PushPopSingle)
{
    auto in = makeSample(42, 3.14f, 1000);
    SensorReading out{};

    EXPECT_TRUE(m_rb.push(in));
    EXPECT_TRUE(m_rb.pop(out));
    EXPECT_EQ(out, in);
}

TEST_F(SensorRingBufferTest, BulkBatch)
{
    SensorReading batch[4] = {
        makeSample(1, 1.0f, 100),
        makeSample(2, 2.0f, 200),
        makeSample(3, 3.0f, 300),
        makeSample(4, 4.0f, 400),
    };
    EXPECT_TRUE(m_rb.write(batch, 4));

    SensorReading out[4]{};
    EXPECT_TRUE(m_rb.read(out, 4));
    for (int i = 0; i < 4; ++i)
    {
        EXPECT_EQ(out[i], batch[i]);
    }
}

TEST_F(SensorRingBufferTest, PeekThenRead)
{
    m_rb.push(makeSample(1, 10.0f, 100));
    m_rb.push(makeSample(2, 20.0f, 200));

    SensorReading peeked[2]{};
    EXPECT_TRUE(m_rb.peek(peeked, 2));
    EXPECT_EQ(m_rb.readAvailable(), 2u); // Not consumed.

    SensorReading actual[2]{};
    EXPECT_TRUE(m_rb.read(actual, 2));
    EXPECT_EQ(actual[0], peeked[0]);
    EXPECT_EQ(actual[1], peeked[1]);
}

TEST_F(SensorRingBufferTest, WraparoundPreservesFieldValues)
{
    // Advance past midpoint.
    for (int i = 0; i < 14; ++i)
    {
        m_rb.push(makeSample(i, 0, 0));
        SensorReading tmp{};
        m_rb.pop(tmp);
    }

    // Write entries that wrap around the buffer end.
    for (int i = 0; i < 16; ++i)
    {
        auto s = makeSample(i + 100, static_cast<float>(i) * 1.1f,
                            static_cast<uint64_t>(i) * 1000);
        EXPECT_TRUE(m_rb.push(s));
    }

    for (int i = 0; i < 16; ++i)
    {
        SensorReading out{};
        EXPECT_TRUE(m_rb.pop(out));
        EXPECT_EQ(out.sensorId, static_cast<uint32_t>(i + 100));
        EXPECT_FLOAT_EQ(out.value, static_cast<float>(i) * 1.1f);
        EXPECT_EQ(out.timestamp, static_cast<uint64_t>(i) * 1000);
    }
}

// ---------------------------------------------------------------------------
// Fixture for PacketHeader (protocol-style struct)
// ---------------------------------------------------------------------------

class PacketHeaderRingBufferTest : public ::testing::Test
{
protected:
    ms::spsc::RingBuffer<PacketHeader, 32> m_rb;
};

TEST_F(PacketHeaderRingBufferTest, ProtocolHeaders)
{
    PacketHeader headers[] = {
        {0x01, 0x00, 64, 1},
        {0x02, 0x80, 128, 2},
        {0x03, 0xFF, 1500, 3},
    };
    EXPECT_TRUE(m_rb.write(headers, 3));

    PacketHeader out[3]{};
    EXPECT_TRUE(m_rb.read(out, 3));
    for (int i = 0; i < 3; ++i)
    {
        EXPECT_EQ(out[i], headers[i]);
    }
}

TEST_F(PacketHeaderRingBufferTest, SequenceNumberIntegrity)
{
    // Simulate a stream of sequenced packets.
    for (uint32_t seq = 0; seq < 100; ++seq)
    {
        PacketHeader h{0x01, 0x00, 100, seq};
        while (!m_rb.push(h))
        {
            // Drain one to make room.
            PacketHeader tmp{};
            m_rb.pop(tmp);
        }
    }

    // Drain remaining and verify they are in order.
    uint32_t lastSeq = 0;
    bool first = true;
    while (m_rb.readAvailable() > 0)
    {
        PacketHeader out{};
        m_rb.pop(out);
        if (!first)
        {
            EXPECT_GT(out.sequenceNumber, lastSeq);
        }
        lastSeq = out.sequenceNumber;
        first = false;
    }
}

// ---------------------------------------------------------------------------
// FixedString (struct with embedded array)
// ---------------------------------------------------------------------------

class FixedStringRingBufferTest : public ::testing::Test
{
protected:
    ms::spsc::RingBuffer<FixedString, 8> m_rb;

    FixedString makeString(const char *s)
    {
        FixedString fs{};
        fs.length = static_cast<uint32_t>(std::strlen(s));
        std::memcpy(fs.data, s, fs.length);
        return fs;
    }
};

TEST_F(FixedStringRingBufferTest, StoreAndRetrieveStrings)
{
    auto hello = makeString("hello");
    auto world = makeString("world");

    EXPECT_TRUE(m_rb.push(hello));
    EXPECT_TRUE(m_rb.push(world));

    FixedString out{};
    EXPECT_TRUE(m_rb.pop(out));
    EXPECT_EQ(out, hello);

    EXPECT_TRUE(m_rb.pop(out));
    EXPECT_EQ(out, world);
}

// ---------------------------------------------------------------------------
// NestedStruct (struct containing other structs)
// ---------------------------------------------------------------------------

class NestedStructRingBufferTest : public ::testing::Test
{
protected:
    ms::spsc::RingBuffer<NestedStruct, 8> m_rb;
};

TEST_F(NestedStructRingBufferTest, NestedFieldsPreserved)
{
    NestedStruct in{};
    in.position = {1.0f, 2.0f};
    in.velocity = {0.5f, -0.5f};
    in.id = 12345;

    EXPECT_TRUE(m_rb.push(in));

    NestedStruct out{};
    EXPECT_TRUE(m_rb.pop(out));
    EXPECT_EQ(out, in);
}

TEST_F(NestedStructRingBufferTest, BulkWraparound)
{
    // Advance past midpoint.
    for (int i = 0; i < 7; ++i)
    {
        NestedStruct ns{};
        ns.id = i;
        m_rb.push(ns);
        NestedStruct tmp{};
        m_rb.pop(tmp);
    }

    // Bulk write that wraps.
    NestedStruct batch[6];
    for (int i = 0; i < 6; ++i)
    {
        batch[i].position = {static_cast<float>(i), 0};
        batch[i].velocity = {0, static_cast<float>(i)};
        batch[i].id = 500 + i;
    }
    EXPECT_TRUE(m_rb.write(batch, 6));

    NestedStruct out[6]{};
    EXPECT_TRUE(m_rb.read(out, 6));
    for (int i = 0; i < 6; ++i)
    {
        EXPECT_EQ(out[i], batch[i]);
    }
}
