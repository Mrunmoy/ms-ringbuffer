#include <cstdint>
#include <cstdio>

#include "spsc/RingBuffer.h"

namespace
{

    struct Payload64
    {
        uint8_t m_bytes[64];
    };

    template <typename T>
    static void print_sizeof(const char *name)
    {
        std::printf("%s sizeof=%zu\n", name, sizeof(T));
    }

} // namespace

int main()
{
    using namespace ms::spsc;

    print_sizeof<RingBuffer<uint8_t, 1024>>("RingBuffer<uint8_t,1024>");
    print_sizeof<RingBuffer<uint64_t, 65536>>("RingBuffer<uint64_t,65536>");
    print_sizeof<RingBuffer<Payload64, 4096>>("RingBuffer<Payload64,4096>");

    print_sizeof<ByteRingBuffer<65536>>("ByteRingBuffer<65536>");

    return 0;
}