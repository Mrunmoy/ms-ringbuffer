// Tests for ByteRingBuffer (RingBuffer<uint8_t, N>).
//
// Demonstrates the IPC / shared-memory use case where the ring buffer
// carries raw bytes, typically length-prefixed frames or serialized messages.

#include <spsc/RingBuffer.h>

#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

#include <gtest/gtest.h>

// ---------------------------------------------------------------------------
// Fixture
// ---------------------------------------------------------------------------

class ByteRingBufferTest : public ::testing::Test
{
protected:
    static constexpr uint32_t kSize = 256;
    ms::spsc::ByteRingBuffer<kSize> m_rb;

    // Write a length-prefixed frame: [uint32_t length][payload bytes].
    bool writeFrame(const void *payload, uint32_t len)
    {
        uint32_t leLen = len; // assume LE for simplicity
        if (m_rb.writeAvailable() < sizeof(leLen) + len)
            return false;
        m_rb.write(reinterpret_cast<const uint8_t *>(&leLen), sizeof(leLen));
        m_rb.write(reinterpret_cast<const uint8_t *>(payload), len);
        return true;
    }

    // Read a length-prefixed frame.
    bool readFrame(std::vector<uint8_t> &out)
    {
        uint32_t len = 0;
        if (m_rb.readAvailable() < sizeof(len))
            return false;
        // Peek the length first.
        if (!m_rb.peek(reinterpret_cast<uint8_t *>(&len), sizeof(len)))
            return false;
        if (m_rb.readAvailable() < sizeof(len) + len)
            return false;
        m_rb.skip(sizeof(len));
        out.resize(len);
        m_rb.read(out.data(), len);
        return true;
    }
};

// -- Basic byte write/read ---------------------------------------------------

TEST_F(ByteRingBufferTest, WriteReadRawBytes)
{
    const char *msg = "hello, ring buffer!";
    uint32_t len = static_cast<uint32_t>(std::strlen(msg));

    EXPECT_TRUE(m_rb.write(reinterpret_cast<const uint8_t *>(msg), len));
    EXPECT_EQ(m_rb.readAvailable(), len);

    std::vector<uint8_t> buf(len);
    EXPECT_TRUE(m_rb.read(buf.data(), len));
    EXPECT_EQ(std::memcmp(buf.data(), msg, len), 0);
}

// -- Length-prefixed framing -------------------------------------------------

TEST_F(ByteRingBufferTest, LengthPrefixedFrames)
{
    std::string msg1 = "first message";
    std::string msg2 = "second msg";
    std::string msg3 = "third";

    EXPECT_TRUE(writeFrame(msg1.data(), msg1.size()));
    EXPECT_TRUE(writeFrame(msg2.data(), msg2.size()));
    EXPECT_TRUE(writeFrame(msg3.data(), msg3.size()));

    std::vector<uint8_t> frame;

    EXPECT_TRUE(readFrame(frame));
    EXPECT_EQ(std::string(frame.begin(), frame.end()), msg1);

    EXPECT_TRUE(readFrame(frame));
    EXPECT_EQ(std::string(frame.begin(), frame.end()), msg2);

    EXPECT_TRUE(readFrame(frame));
    EXPECT_EQ(std::string(frame.begin(), frame.end()), msg3);
}

// -- Wraparound with frames --------------------------------------------------

TEST_F(ByteRingBufferTest, FrameWraparound)
{
    // Fill and drain most of the buffer to advance the position.
    std::vector<uint8_t> filler(200, 0xAA);
    EXPECT_TRUE(m_rb.write(filler.data(), filler.size()));
    m_rb.skip(filler.size());

    // Now write a frame that will wrap around the end.
    std::string msg = "this frame wraps around the ring buffer boundary";
    EXPECT_TRUE(writeFrame(msg.data(), msg.size()));

    std::vector<uint8_t> frame;
    EXPECT_TRUE(readFrame(frame));
    EXPECT_EQ(std::string(frame.begin(), frame.end()), msg);
}

// -- Multiple variable-length messages ---------------------------------------

TEST_F(ByteRingBufferTest, VariableLengthMessages)
{
    std::vector<std::string> messages = {
        "a",
        "bb",
        "ccc",
        std::string(50, 'x'),
        "short",
        std::string(30, 'z'),
    };

    for (const auto &m : messages)
    {
        EXPECT_TRUE(writeFrame(m.data(), m.size()));
    }

    for (const auto &m : messages)
    {
        std::vector<uint8_t> frame;
        EXPECT_TRUE(readFrame(frame));
        EXPECT_EQ(std::string(frame.begin(), frame.end()), m);
    }
}

// -- ByteRingBuffer alias works ----------------------------------------------

TEST_F(ByteRingBufferTest, AliasMatchesFullType)
{
    // Verify ByteRingBuffer<N> is truly RingBuffer<uint8_t, N>.
    static_assert(
        std::is_same_v<
            ms::spsc::ByteRingBuffer<64>,
            ms::spsc::RingBuffer<uint8_t, 64>>,
        "ByteRingBuffer should be an alias for RingBuffer<uint8_t, N>");
}

// -- Binary data preservation ------------------------------------------------

TEST_F(ByteRingBufferTest, BinaryDataWithNullBytes)
{
    // Data containing null bytes — must not be truncated.
    uint8_t binary[] = {0x00, 0x01, 0x00, 0xFF, 0x00, 0x80, 0x00};
    uint32_t len = sizeof(binary);

    EXPECT_TRUE(m_rb.write(binary, len));

    uint8_t out[sizeof(binary)]{};
    EXPECT_TRUE(m_rb.read(out, len));
    EXPECT_EQ(std::memcmp(out, binary, len), 0);
}

// -- Exact capacity fill with bytes ------------------------------------------

TEST_F(ByteRingBufferTest, ExactCapacityFill)
{
    std::vector<uint8_t> data(kSize);
    for (uint32_t i = 0; i < kSize; ++i)
        data[i] = static_cast<uint8_t>(i);

    EXPECT_TRUE(m_rb.write(data.data(), kSize));
    EXPECT_TRUE(m_rb.isFull());
    EXPECT_FALSE(m_rb.write(data.data(), 1)); // No room for even 1 byte.

    std::vector<uint8_t> out(kSize);
    EXPECT_TRUE(m_rb.read(out.data(), kSize));
    EXPECT_EQ(data, out);
}
