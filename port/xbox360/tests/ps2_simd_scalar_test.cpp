#include "ps2_simd_scalar.h"

#include <cassert>
#include <cstdint>

using ps2::xbox360::simd::Vector128;
using ps2::xbox360::simd::readLane;
using ps2::xbox360::simd::writeLane;

int main()
{
    using namespace ps2::xbox360::simd;
    Vector128 a{}, b{};

    // Wrapping arithmetic independently in byte, halfword and word lanes.
    writeLane<std::uint8_t>(a, 0u, 0xFFu);
    writeLane<std::uint8_t>(b, 0u, 0x01u);
    assert(readLane<std::uint8_t>(addLanes<std::uint8_t>(a, b), 0u) == 0x00u);
    assert(readLane<std::uint8_t>(subtractLanes<std::uint8_t>(a, b), 0u) == 0xFEu);
    a = {}; b = {};
    writeLane<std::uint16_t>(a, 0u, 0xFFFFu);
    writeLane<std::uint16_t>(b, 0u, 0x0001u);
    assert(readLane<std::uint16_t>(addLanes<std::uint16_t>(a, b), 0u) == 0u);
    assert(readLane<std::uint16_t>(subtractLanes<std::uint16_t>(b, a), 0u) == 2u);
    a = {}; b = {};
    writeLane<std::uint32_t>(a, 0u, 0xFFFFFFFFu);
    writeLane<std::uint32_t>(b, 0u, 1u);
    assert(readLane<std::uint32_t>(addLanes<std::uint32_t>(a, b), 0u) == 0u);
    assert(readLane<std::uint32_t>(subtractLanes<std::uint32_t>(b, a), 0u) == 2u);

    // Bytewise logical operations.
    a = {}; b = {};
    a.bytes[0] = 0xF0u; b.bytes[0] = 0x0Fu;
    assert(bitAnd(a, b).bytes[0] == 0u);
    assert(bitOr(a, b).bytes[0] == 0xFFu);
    assert(bitXor(a, b).bytes[0] == 0xFFu);
    assert(bitNor(a, b).bytes[0] == 0u);
    assert(bitNor(Vector128{}, Vector128{}).bytes[0] == 0xFFu);

    // Signed comparison and min/max must not use unsigned ordering.
    a = {}; b = {};
    writeLane<std::uint32_t>(a, 0u, 0xFFFFFFFFu); // -1
    writeLane<std::uint32_t>(b, 0u, 1u);          // +1
    assert(readLane<std::uint32_t>(compareGreaterSigned<std::uint32_t>(b, a), 0u) == 0xFFFFFFFFu);
    assert(readLane<std::uint32_t>(compareGreaterSigned<std::uint32_t>(a, b), 0u) == 0u);
    assert(readLane<std::uint32_t>(minSigned<std::uint32_t>(a, b), 0u) == 0xFFFFFFFFu);
    assert(readLane<std::uint32_t>(maxSigned<std::uint32_t>(a, b), 0u) == 1u);
    a = {}; b = {};
    writeLane<std::uint16_t>(a, 0u, 0x8000u);
    writeLane<std::uint16_t>(b, 0u, 0x7FFFu);
    assert(readLane<std::uint16_t>(minSigned<std::uint16_t>(a, b), 0u) == 0x8000u);
    assert(readLane<std::uint16_t>(maxSigned<std::uint16_t>(a, b), 0u) == 0x7FFFu);
    a = {}; b = {};
    writeLane<std::uint8_t>(a, 0u, 0x80u);
    writeLane<std::uint8_t>(b, 0u, 0x7Fu);
    assert(readLane<std::uint8_t>(compareGreaterSigned<std::uint8_t>(b, a), 0u) == 0xFFu);
    assert(readLane<std::uint8_t>(minSigned<std::uint8_t>(a, b), 0u) == 0x80u);
    assert(readLane<std::uint8_t>(maxSigned<std::uint8_t>(a, b), 0u) == 0x7Fu);

    // Packed absolute value leaves the signed minimum as its original bit pattern.
    a = {};
    writeLane<std::uint8_t>(a, 0u, 0x80u);
    writeLane<std::uint8_t>(a, 1u, 0xFEu);
    const auto abs8 = absoluteSigned<std::uint8_t>(a);
    assert(readLane<std::uint8_t>(abs8, 0u) == 0x80u);
    assert(readLane<std::uint8_t>(abs8, 1u) == 2u);
    a = {};
    writeLane<std::uint16_t>(a, 0u, 0x8000u);
    writeLane<std::uint16_t>(a, 1u, 0xFFFEu);
    const auto abs16 = absoluteSigned<std::uint16_t>(a);
    assert(readLane<std::uint16_t>(abs16, 0u) == 0x8000u);
    assert(readLane<std::uint16_t>(abs16, 1u) == 2u);
    a = {};
    writeLane<std::uint32_t>(a, 0u, 0x80000000u);
    writeLane<std::uint32_t>(a, 1u, 0xFFFFFFFEu);
    const auto abs32 = absoluteSigned<std::uint32_t>(a);
    assert(readLane<std::uint32_t>(abs32, 0u) == 0x80000000u);
    assert(readLane<std::uint32_t>(abs32, 1u) == 2u);

    // Signed saturation at both ends of the 32-bit range.
    a = {}; b = {};
    writeLane<std::uint32_t>(a, 0u, 0x7FFFFFFFu);
    writeLane<std::uint32_t>(b, 0u, 1u);
    writeLane<std::uint32_t>(a, 1u, 0x80000000u);
    writeLane<std::uint32_t>(b, 1u, 0xFFFFFFFFu);
    const auto satAdd = addSaturatingSigned32(a, b);
    assert(readLane<std::uint32_t>(satAdd, 0u) == 0x7FFFFFFFu);
    assert(readLane<std::uint32_t>(satAdd, 1u) == 0x80000000u);
    Vector128 subA{}, subB{};
    writeLane<std::uint32_t>(subA, 0u, 0x80000000u);
    writeLane<std::uint32_t>(subB, 0u, 1u);
    writeLane<std::uint32_t>(subA, 1u, 0x7FFFFFFFu);
    writeLane<std::uint32_t>(subB, 1u, 0xFFFFFFFFu);
    const auto satSub = subtractSaturatingSigned32(subA, subB);
    assert(readLane<std::uint32_t>(satSub, 0u) == 0x80000000u);
    assert(readLane<std::uint32_t>(satSub, 1u) == 0x7FFFFFFFu);

    // Saturating signed byte/halfword arithmetic and unsigned byte/word arithmetic.
    Vector128 sat8a{}, sat8b{};
    writeLane<std::uint8_t>(sat8a, 0u, 0x7Fu);
    writeLane<std::uint8_t>(sat8b, 0u, 0x01u);
    writeLane<std::uint8_t>(sat8a, 1u, 0x80u);
    writeLane<std::uint8_t>(sat8b, 1u, 0xFFu);
    writeLane<std::uint8_t>(sat8a, 2u, 0x80u);
    writeLane<std::uint8_t>(sat8b, 2u, 0x01u);
    assert(readLane<std::uint8_t>(addSaturatingSignedLanes<std::uint8_t>(sat8a, sat8b), 0u) == 0x7Fu);
    assert(readLane<std::uint8_t>(addSaturatingSignedLanes<std::uint8_t>(sat8a, sat8b), 1u) == 0x80u);
    assert(readLane<std::uint8_t>(subtractSaturatingSignedLanes<std::uint8_t>(sat8a, sat8b), 0u) == 0x7Eu);
    assert(readLane<std::uint8_t>(subtractSaturatingSignedLanes<std::uint8_t>(sat8a, sat8b), 1u) == 0x81u);
    assert(readLane<std::uint8_t>(subtractSaturatingSignedLanes<std::uint8_t>(sat8a, sat8b), 2u) == 0x80u);

    Vector128 sat16a{}, sat16b{};
    writeLane<std::uint16_t>(sat16a, 0u, 0x7FFFu);
    writeLane<std::uint16_t>(sat16b, 0u, 0x0001u);
    writeLane<std::uint16_t>(sat16a, 1u, 0x8000u);
    writeLane<std::uint16_t>(sat16b, 1u, 0xFFFFu);
    writeLane<std::uint16_t>(sat16a, 2u, 0x8000u);
    writeLane<std::uint16_t>(sat16b, 2u, 0x0001u);
    assert(readLane<std::uint16_t>(addSaturatingSignedLanes<std::uint16_t>(sat16a, sat16b), 0u) == 0x7FFFu);
    assert(readLane<std::uint16_t>(addSaturatingSignedLanes<std::uint16_t>(sat16a, sat16b), 1u) == 0x8000u);
    assert(readLane<std::uint16_t>(subtractSaturatingSignedLanes<std::uint16_t>(sat16a, sat16b), 0u) == 0x7FFEu);
    assert(readLane<std::uint16_t>(subtractSaturatingSignedLanes<std::uint16_t>(sat16a, sat16b), 1u) == 0x8001u);
    assert(readLane<std::uint16_t>(subtractSaturatingSignedLanes<std::uint16_t>(sat16a, sat16b), 2u) == 0x8000u);

    Vector128 unsigned8a{}, unsigned8b{};
    writeLane<std::uint8_t>(unsigned8a, 0u, 0xFEu);
    writeLane<std::uint8_t>(unsigned8b, 0u, 0x02u);
    writeLane<std::uint8_t>(unsigned8a, 1u, 0x01u);
    writeLane<std::uint8_t>(unsigned8b, 1u, 0x02u);
    assert(readLane<std::uint8_t>(addSaturatingUnsignedLanes<std::uint8_t>(unsigned8a, unsigned8b), 0u) == 0xFFu);
    assert(readLane<std::uint8_t>(subtractSaturatingUnsignedLanes<std::uint8_t>(unsigned8a, unsigned8b), 1u) == 0u);

    Vector128 unsigned32a{}, unsigned32b{};
    writeLane<std::uint32_t>(unsigned32a, 0u, 0xFFFFFFFFu);
    writeLane<std::uint32_t>(unsigned32b, 0u, 1u);
    writeLane<std::uint32_t>(unsigned32b, 1u, 1u);
    assert(readLane<std::uint32_t>(addSaturatingUnsignedLanes<std::uint32_t>(unsigned32a, unsigned32b), 0u) == 0xFFFFFFFFu);
    assert(readLane<std::uint32_t>(subtractSaturatingUnsignedLanes<std::uint32_t>(unsigned32a, unsigned32b), 1u) == 0u);

    // PMADDH: all eight products accumulate into the documented HI/LO lanes.
    Vector128 maddA{}, maddB{}, initialHi{}, initialLo{};
    const std::uint16_t lhsHalfwords[8] = {
        2u, 0xFFFDu, 4u, 0xFFFBu, 6u, 0xFFF9u, 8u, 0xFFF7u};
    const std::uint16_t rhsHalfwords[8] = {
        10u, 20u, 0xFFE2u, 0xFFD8u, 50u, 60u, 0xFFBAu, 0xFFB0u};
    for (std::size_t i = 0; i < 8u; ++i)
    {
        writeLane<std::uint16_t>(maddA, i, lhsHalfwords[i]);
        writeLane<std::uint16_t>(maddB, i, rhsHalfwords[i]);
    }
    for (std::size_t i = 0; i < 4u; ++i)
    {
        writeLane<std::uint32_t>(initialLo, i, static_cast<std::uint32_t>(i + 1u));
        writeLane<std::uint32_t>(initialHi, i, static_cast<std::uint32_t>(i + 5u));
    }
    const auto madd = pmaddh(maddA, maddB, initialHi, initialLo);
    assert(readLane<std::uint32_t>(madd.lo, 0u) == 21u);
    assert(readLane<std::uint32_t>(madd.lo, 1u) == 0xFFFFFFC6u);
    assert(readLane<std::uint32_t>(madd.hi, 0u) == 0xFFFFFF8Du);
    assert(readLane<std::uint32_t>(madd.hi, 1u) == 206u);
    assert(readLane<std::uint32_t>(madd.lo, 2u) == 303u);
    assert(readLane<std::uint32_t>(madd.lo, 3u) == 0xFFFFFE60u);
    assert(readLane<std::uint32_t>(madd.hi, 2u) == 0xFFFFFDD7u);
    assert(readLane<std::uint32_t>(madd.hi, 3u) == 728u);
    assert(readLane<std::uint32_t>(madd.destination, 0u) == 21u);
    assert(readLane<std::uint32_t>(madd.destination, 1u) == 0xFFFFFF8Du);
    assert(readLane<std::uint32_t>(madd.destination, 2u) == 303u);
    assert(readLane<std::uint32_t>(madd.destination, 3u) == 0xFFFFFDD7u);

    // Accumulator words wrap independently at 32 bits.
    Vector128 wrapA{}, wrapB{}, wrapLo{};
    writeLane<std::uint16_t>(wrapA, 0u, 1u);
    writeLane<std::uint16_t>(wrapB, 0u, 1u);
    writeLane<std::uint32_t>(wrapLo, 0u, 0x7FFFFFFFu);
    const auto wrappedMadd = pmaddh(wrapA, wrapB, Vector128{}, wrapLo);
    assert(readLane<std::uint32_t>(wrappedMadd.lo, 0u) == 0x80000000u);
    assert(readLane<std::uint32_t>(wrappedMadd.destination, 0u) == 0x80000000u);

    // Variable shifts use the low five bits of each count.
    a = {}; b = {};
    writeLane<std::uint32_t>(a, 0u, 0x80000000u);
    writeLane<std::uint32_t>(a, 1u, 0xFFFFFFF8u);
    writeLane<std::uint32_t>(b, 0u, 1u);
    writeLane<std::uint32_t>(b, 1u, 1u);
    writeLane<std::uint32_t>(b, 2u, 33u);
    assert(readLane<std::uint32_t>(shiftLeftVariable32(a, b), 0u) == 0u);
    assert(readLane<std::uint32_t>(shiftRightLogicalVariable32(a, b), 0u) == 0x40000000u);
    assert(readLane<std::uint32_t>(shiftRightArithmeticVariable32(a, b), 1u) == 0xFFFFFFFCu);
    assert(readLane<std::uint32_t>(shiftRightLogicalVariable32(a, b), 2u) == 0u);

    // Saturating 32-to-16 pack.
    a = {}; b = {};
    writeLane<std::uint32_t>(a, 0u, 0x00010000u);
    writeLane<std::uint32_t>(a, 1u, 0xFFFF0000u);
    writeLane<std::uint32_t>(a, 2u, 5u);
    writeLane<std::uint32_t>(a, 3u, 0xFFFFFFFBu);
    writeLane<std::uint32_t>(b, 0u, 0x80000000u);
    writeLane<std::uint32_t>(b, 1u, 0x7FFFFFFFu);
    const auto packed16 = packSigned32To16(a, b);
    assert(readLane<std::uint16_t>(packed16, 0u) == 0x7FFFu);
    assert(readLane<std::uint16_t>(packed16, 1u) == 0x8000u);
    assert(readLane<std::uint16_t>(packed16, 2u) == 5u);
    assert(readLane<std::uint16_t>(packed16, 3u) == 0xFFFBu);
    assert(readLane<std::uint16_t>(packed16, 4u) == 0x8000u);
    assert(readLane<std::uint16_t>(packed16, 5u) == 0x7FFFu);

    // Interleave and PPAC even-lane order for all lane widths.
    a = {}; b = {};
    for (std::size_t i = 0; i < 16u; ++i)
    {
        a.bytes[i] = static_cast<std::uint8_t>(i);
        b.bytes[i] = static_cast<std::uint8_t>(0x80u + i);
    }
    const auto low8 = interleaveLow<std::uint8_t>(a, b);
    const auto high8 = interleaveHigh<std::uint8_t>(a, b);
    assert(readLane<std::uint8_t>(low8, 0u) == 0x80u);
    assert(readLane<std::uint8_t>(low8, 1u) == 0u);
    assert(readLane<std::uint8_t>(high8, 0u) == 0x88u);
    assert(readLane<std::uint8_t>(high8, 1u) == 8u);
    const auto low16 = interleaveLow<std::uint16_t>(a, b);
    const auto high16 = interleaveHigh<std::uint16_t>(a, b);
    assert(readLane<std::uint16_t>(low16, 0u) == 0x8180u);
    assert(readLane<std::uint16_t>(low16, 1u) == 0x0100u);
    assert(readLane<std::uint16_t>(high16, 0u) == 0x8988u);
    assert(readLane<std::uint16_t>(high16, 1u) == 0x0908u);
    const auto even8 = packEvenLanes<std::uint8_t>(a, b);
    assert(readLane<std::uint8_t>(even8, 0u) == 0x80u);
    assert(readLane<std::uint8_t>(even8, 8u) == 0u);
    const auto even16 = packEvenLanes<std::uint16_t>(a, b);
    assert(readLane<std::uint16_t>(even16, 0u) == 0x8180u);
    assert(readLane<std::uint16_t>(even16, 4u) == 0x0100u);
    const auto even32 = packEvenLanes<std::uint32_t>(a, b);
    assert(readLane<std::uint32_t>(even32, 0u) == 0x83828180u);
    assert(readLane<std::uint32_t>(even32, 1u) == 0x8B8A8988u);
    assert(readLane<std::uint32_t>(even32, 2u) == 0x03020100u);
    assert(readLane<std::uint32_t>(even32, 3u) == 0x0B0A0908u);

    // Equality masks and guest little-endian lane bytes.
    a = {}; b = {};
    writeLane<std::uint32_t>(a, 1u, 0x12345678u);
    writeLane<std::uint32_t>(b, 1u, 0x12345678u);
    writeLane<std::uint32_t>(b, 0u, 1u); // Keep lane 0 unequal; lane 1 must match.
    const auto equal = compareEqualLanes<std::uint32_t>(a, b);
    assert(readLane<std::uint32_t>(equal, 1u) == 0xFFFFFFFFu);
    assert(readLane<std::uint32_t>(equal, 0u) == 0u);
    Vector128 endian{};
    writeLane<std::uint32_t>(endian, 0u, 0x12345678u);
    assert(endian.bytes[0] == 0x78u && endian.bytes[1] == 0x56u &&
           endian.bytes[2] == 0x34u && endian.bytes[3] == 0x12u);
    assert(readLane<std::uint32_t>(endian, 0u) == 0x12345678u);
    return 0;
}
