#include "ps2_simd_scalar.h"

#include <cassert>
#include <cstdint>

using ps2::xbox360::simd::Vector128;
using ps2::xbox360::simd::readLane;
using ps2::xbox360::simd::writeLane;

int main()
{
    Vector128 a{};
    Vector128 b{};

    writeLane<std::uint8_t>(a, 0u, 0xFFu);
    writeLane<std::uint8_t>(b, 0u, 0x01u);
    auto sum8 = ps2::xbox360::simd::addLanes<std::uint8_t>(a, b);
    auto sub8 = ps2::xbox360::simd::subtractLanes<std::uint8_t>(a, b);
    assert(readLane<std::uint8_t>(sum8, 0u) == 0x00u);
    assert(readLane<std::uint8_t>(sub8, 0u) == 0xFEu);

    a = {};
    b = {};
    writeLane<std::uint16_t>(a, 0u, 0xFFFFu);
    writeLane<std::uint16_t>(b, 0u, 0x0001u);
    auto sum16 = ps2::xbox360::simd::addLanes<std::uint16_t>(a, b);
    auto sub16 = ps2::xbox360::simd::subtractLanes<std::uint16_t>(b, a);
    assert(readLane<std::uint16_t>(sum16, 0u) == 0x0000u);
    assert(readLane<std::uint16_t>(sub16, 0u) == 0x0002u);

    a = {};
    b = {};
    writeLane<std::uint32_t>(a, 0u, 0xFFFFFFFFu);
    writeLane<std::uint32_t>(b, 0u, 0x00000001u);
    auto sum32 = ps2::xbox360::simd::addLanes<std::uint32_t>(a, b);
    auto sub32 = ps2::xbox360::simd::subtractLanes<std::uint32_t>(b, a);
    assert(readLane<std::uint32_t>(sum32, 0u) == 0x00000000u);
    assert(readLane<std::uint32_t>(sub32, 0u) == 0x00000002u);
    assert(sum32.bytes[0] == 0u && sum32.bytes[1] == 0u &&
           sum32.bytes[2] == 0u && sum32.bytes[3] == 0u);

    a = {};
    b = {};
    a.bytes[0] = 0xF0u;
    b.bytes[0] = 0x0Fu;
    assert(ps2::xbox360::simd::bitAnd(a, b).bytes[0] == 0x00u);
    assert(ps2::xbox360::simd::bitOr(a, b).bytes[0] == 0xFFu);
    assert(ps2::xbox360::simd::bitXor(a, b).bytes[0] == 0xFFu);
    assert(ps2::xbox360::simd::bitNor(a, b).bytes[0] == 0x00u);
    writeLane<std::uint32_t>(a, 1u, 0x12345678u);
    writeLane<std::uint32_t>(b, 1u, 0x12345678u);
    auto equal = ps2::xbox360::simd::compareEqualLanes<std::uint32_t>(a, b);
    assert(readLane<std::uint32_t>(equal, 1u) == 0xFFFFFFFFu);
    assert(readLane<std::uint32_t>(equal, 0u) == 0x00000000u);

    a = {};
    b = {};
    for (std::size_t i = 0; i < 4u; ++i)
    {
        writeLane<std::uint32_t>(a, i, static_cast<std::uint32_t>(10u + i));
        writeLane<std::uint32_t>(b, i, static_cast<std::uint32_t>(20u + i));
    }
    auto low = ps2::xbox360::simd::interleaveLow<std::uint32_t>(a, b);
    auto high = ps2::xbox360::simd::interleaveHigh<std::uint32_t>(a, b);
    assert(readLane<std::uint32_t>(low, 0u) == 20u);
    assert(readLane<std::uint32_t>(low, 1u) == 10u);
    assert(readLane<std::uint32_t>(low, 2u) == 21u);
    assert(readLane<std::uint32_t>(low, 3u) == 11u);
    assert(readLane<std::uint32_t>(high, 0u) == 22u);
    assert(readLane<std::uint32_t>(high, 1u) == 12u);
    assert(readLane<std::uint32_t>(high, 2u) == 23u);
    assert(readLane<std::uint32_t>(high, 3u) == 13u);

    Vector128 endian{};
    writeLane<std::uint32_t>(endian, 0u, 0x12345678u);
    assert(endian.bytes[0] == 0x78u && endian.bytes[1] == 0x56u &&
           endian.bytes[2] == 0x34u && endian.bytes[3] == 0x12u);
    assert(readLane<std::uint32_t>(endian, 0u) == 0x12345678u);
    return 0;
}
