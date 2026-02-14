// Basic usage of ms::spsc::RingBuffer with different data types.
//
// Demonstrates: push/pop, bulk write/read, peek, skip, reset,
// and the ByteRingBuffer alias for raw byte streams.

#include <spsc/RingBuffer.h>

#include <cstdint>
#include <cstdio>
#include <cstring>

// A trivially copyable struct — any such type works with the ring buffer.
struct SensorReading
{
    uint32_t sensorId;
    float value;
    uint64_t timestamp;
};

int main()
{
    // ── 1. Integer ring buffer ──────────────────────────────────────
    printf("=== Integer ring buffer ===\n");

    ms::spsc::RingBuffer<int, 8> intBuf;

    // Push individual elements.
    intBuf.push(10);
    intBuf.push(20);
    intBuf.push(30);
    printf("After pushing 3 elements: available=%u, capacity=%u\n",
           intBuf.readAvailable(), intBuf.capacity());

    // Pop them back.
    int val;
    intBuf.pop(val);
    printf("Popped: %d\n", val);

    // Peek without consuming.
    intBuf.peek(&val, 1);
    printf("Peeked: %d (still in buffer, available=%u)\n",
           val, intBuf.readAvailable());

    // Skip one element.
    intBuf.skip(1);
    intBuf.pop(val);
    printf("After skip(1), popped: %d\n", val);

    // ── 2. Bulk write / read ────────────────────────────────────────
    printf("\n=== Bulk write/read ===\n");

    intBuf.reset();
    int src[] = {100, 200, 300, 400, 500};
    intBuf.write(src, 5);
    printf("Wrote 5 elements in bulk\n");

    int dst[5] = {};
    intBuf.read(dst, 5);
    printf("Read back:");
    for (int i = 0; i < 5; ++i)
        printf(" %d", dst[i]);
    printf("\n");

    // ── 3. Struct ring buffer ───────────────────────────────────────
    printf("\n=== Struct ring buffer ===\n");

    ms::spsc::RingBuffer<SensorReading, 16> sensorBuf;

    SensorReading readings[] = {
        {1, 23.5f, 1000},
        {2, 18.2f, 1001},
        {3, 42.0f, 1002},
    };
    sensorBuf.write(readings, 3);
    printf("Wrote 3 sensor readings\n");

    SensorReading out;
    while (sensorBuf.pop(out))
    {
        printf("  sensor=%u  value=%.1f  time=%lu\n",
               out.sensorId, out.value, (unsigned long)out.timestamp);
    }

    // ── 4. Byte ring buffer (IPC-style) ─────────────────────────────
    printf("\n=== Byte ring buffer ===\n");

    ms::spsc::ByteRingBuffer<256> byteBuf;

    // Write a length-prefixed message.
    const char *msg = "hello from ring buffer!";
    uint32_t len = static_cast<uint32_t>(std::strlen(msg));
    byteBuf.write(reinterpret_cast<const uint8_t *>(&len), sizeof(len));
    byteBuf.write(reinterpret_cast<const uint8_t *>(msg), len);
    printf("Wrote %u-byte message\n", len);

    // Read it back.
    uint32_t readLen = 0;
    byteBuf.read(reinterpret_cast<uint8_t *>(&readLen), sizeof(readLen));
    char buf[256] = {};
    byteBuf.read(reinterpret_cast<uint8_t *>(buf), readLen);
    printf("Read back: \"%s\"\n", buf);

    // ── 5. Full/empty checks ────────────────────────────────────────
    printf("\n=== Capacity checks ===\n");

    ms::spsc::RingBuffer<int, 4> smallBuf;
    printf("Empty: %s\n", smallBuf.isEmpty() ? "yes" : "no");

    for (int i = 0; i < 4; ++i)
        smallBuf.push(i);
    printf("Full:  %s  (writeAvailable=%u)\n",
           smallBuf.isFull() ? "yes" : "no", smallBuf.writeAvailable());

    // Push to a full buffer returns false.
    bool ok = smallBuf.push(999);
    printf("Push to full buffer: %s\n", ok ? "succeeded" : "failed (expected)");

    printf("\nDone.\n");
    return 0;
}
