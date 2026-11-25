#include "ps2_ee_special_executor.h"

#include <cassert>
#include <cstdint>

using namespace ps2::xbox360::ee;
namespace simd = ps2::xbox360::simd;

static void writeLow64(simd::Vector128 &value, std::uint64_t bits)
{
    for (std::size_t i = 0; i < 8; ++i)
        value.bytes[i] = static_cast<std::uint8_t>(bits >> (i * 8u));
}

static std::uint64_t readLow64(const simd::Vector128 &value)
{
    std::uint64_t bits = 0;
    for (std::size_t i = 0; i < 8; ++i)
        bits |= static_cast<std::uint64_t>(value.bytes[i]) << (i * 8u);
    return bits;
}

static std::uint32_t encodeSpecial(std::uint8_t rs, std::uint8_t rt, std::uint8_t rd,
                                  std::uint8_t sa, std::uint8_t funct)
{
    return (static_cast<std::uint32_t>(rs) << 21) |
           (static_cast<std::uint32_t>(rt) << 16) |
           (static_cast<std::uint32_t>(rd) << 11) |
           (static_cast<std::uint32_t>(sa) << 6) | funct;
}

int main()
{
    CpuState state{};
    state.pc = 0x1000;
    simd::writeLane<std::uint32_t>(state.gpr[1], 0, 0x12345678u);
    simd::writeLane<std::uint32_t>(state.gpr[2], 0, 0x00FF00FFu);

    auto andDecoded = decodeInstruction(encodeSpecial(1, 2, 3, 0, 0x24));
    assert(executeSpecial(state, andDecoded) == ExecuteStatus::Executed);
    assert(simd::readLane<std::uint32_t>(state.gpr[3], 0) == 0x00340078u);
    assert(state.pc == 0x1004 && state.retiredInstructions == 1);

    auto norDecoded = decodeInstruction(encodeSpecial(1, 2, 4, 0, 0x27));
    assert(executeSpecial(state, norDecoded) == ExecuteStatus::Executed);
    assert(simd::readLane<std::uint32_t>(state.gpr[4], 0) == 0xED00A900u);

    auto movzDecoded = decodeInstruction(encodeSpecial(1, 0, 5, 0, 0x0A));
    assert(executeSpecial(state, movzDecoded) == ExecuteStatus::Executed);
    assert(simd::readLane<std::uint32_t>(state.gpr[5], 0) == 0x12345678u);

    simd::writeLane<std::uint32_t>(state.gpr[6], 0, 0xDEADBEEFu);
    auto movnDecoded = decodeInstruction(encodeSpecial(1, 2, 6, 0, 0x0B));
    assert(executeSpecial(state, movnDecoded) == ExecuteStatus::Executed);
    assert(simd::readLane<std::uint32_t>(state.gpr[6], 0) == 0x12345678u);

    simd::writeLane<std::uint32_t>(state.gpr[2], 0, 0x80000000u);
    auto sraDecoded = decodeInstruction(encodeSpecial(0, 2, 8, 4, 0x03));
    assert(executeSpecial(state, sraDecoded) == ExecuteStatus::Executed);
    assert(simd::readLane<std::uint32_t>(state.gpr[8], 0) == 0xF8000000u);

    simd::writeLane<std::uint32_t>(state.gpr[2], 0, 0x00000010u);
    auto adduDecoded = decodeInstruction(encodeSpecial(1, 2, 9, 0, 0x21));
    assert(executeSpecial(state, adduDecoded) == ExecuteStatus::Executed);
    assert(simd::readLane<std::uint32_t>(state.gpr[9], 0) == 0x12345688u);

    auto subuDecoded = decodeInstruction(encodeSpecial(1, 2, 10, 0, 0x23));
    assert(executeSpecial(state, subuDecoded) == ExecuteStatus::Executed);
    assert(simd::readLane<std::uint32_t>(state.gpr[10], 0) == 0x12345668u);

    simd::writeLane<std::uint32_t>(state.gpr[1], 0, 0xFFFFFFFFu);
    simd::writeLane<std::uint32_t>(state.gpr[2], 0, 1u);
    auto sltDecoded = decodeInstruction(encodeSpecial(1, 2, 11, 0, 0x2A));
    assert(executeSpecial(state, sltDecoded) == ExecuteStatus::Executed);
    assert(simd::readLane<std::uint32_t>(state.gpr[11], 0) == 1u);
    auto sltuDecoded = decodeInstruction(encodeSpecial(1, 2, 12, 0, 0x2B));
    assert(executeSpecial(state, sltuDecoded) == ExecuteStatus::Executed);
    assert(simd::readLane<std::uint32_t>(state.gpr[12], 0) == 0u);

    simd::writeLane<std::uint32_t>(state.gpr[1], 0, 3u);
    simd::writeLane<std::uint32_t>(state.gpr[2], 0, 0x10u);
    auto sllvDecoded = decodeInstruction(encodeSpecial(1, 2, 13, 0, 0x04));
    assert(executeSpecial(state, sllvDecoded) == ExecuteStatus::Executed);
    assert(simd::readLane<std::uint32_t>(state.gpr[13], 0) == 0x80u);
    auto srlvDecoded = decodeInstruction(encodeSpecial(1, 2, 14, 0, 0x06));
    assert(executeSpecial(state, srlvDecoded) == ExecuteStatus::Executed);
    assert(simd::readLane<std::uint32_t>(state.gpr[14], 0) == 2u);
    auto sravDecoded = decodeInstruction(encodeSpecial(1, 2, 15, 0, 0x07));
    assert(executeSpecial(state, sravDecoded) == ExecuteStatus::Executed);
    assert(simd::readLane<std::uint32_t>(state.gpr[15], 0) == 2u);

    // Scalar HI/LO transfers touch only the low 64 bits; MMI banks stay intact.
    for (std::size_t lane = 0; lane < 4; ++lane)
        simd::writeLane<std::uint32_t>(state.gpr[16], lane, 0xA0B0C000u + static_cast<std::uint32_t>(lane));
    for (std::size_t byte = 8; byte < 16; ++byte)
        state.hilo.hi.bytes[byte] = 0xCC;
    for (std::size_t byte = 8; byte < 16; ++byte)
        state.gpr[17].bytes[byte] = 0xDD;
    auto mthiDecoded = decodeInstruction(encodeSpecial(16, 0, 0, 0, 0x11));
    assert(executeSpecial(state, mthiDecoded) == ExecuteStatus::Executed);
    for (std::size_t byte = 0; byte < 8; ++byte)
        assert(state.hilo.hi.bytes[byte] == state.gpr[16].bytes[byte]);
    for (std::size_t byte = 8; byte < 16; ++byte)
        assert(state.hilo.hi.bytes[byte] == 0xCC);
    auto mfhiDecoded = decodeInstruction(encodeSpecial(0, 0, 17, 0, 0x10));
    assert(executeSpecial(state, mfhiDecoded) == ExecuteStatus::Executed);
    for (std::size_t byte = 0; byte < 8; ++byte)
        assert(state.gpr[17].bytes[byte] == state.hilo.hi.bytes[byte]);
    for (std::size_t byte = 8; byte < 16; ++byte)
        assert(state.gpr[17].bytes[byte] == 0xDD);

    for (std::size_t lane = 0; lane < 4; ++lane)
        simd::writeLane<std::uint32_t>(state.gpr[18], lane, 0x11223340u + static_cast<std::uint32_t>(lane));
    for (std::size_t byte = 8; byte < 16; ++byte)
        state.hilo.lo.bytes[byte] = 0xEE;
    for (std::size_t byte = 8; byte < 16; ++byte)
        state.gpr[19].bytes[byte] = 0xBB;
    auto mtloDecoded = decodeInstruction(encodeSpecial(18, 0, 0, 0, 0x13));
    assert(executeSpecial(state, mtloDecoded) == ExecuteStatus::Executed);
    for (std::size_t byte = 0; byte < 8; ++byte)
        assert(state.hilo.lo.bytes[byte] == state.gpr[18].bytes[byte]);
    for (std::size_t byte = 8; byte < 16; ++byte)
        assert(state.hilo.lo.bytes[byte] == 0xEE);
    auto mfloDecoded = decodeInstruction(encodeSpecial(0, 0, 19, 0, 0x12));
    assert(executeSpecial(state, mfloDecoded) == ExecuteStatus::Executed);
    for (std::size_t byte = 0; byte < 8; ++byte)
        assert(state.gpr[19].bytes[byte] == state.hilo.lo.bytes[byte]);
    for (std::size_t byte = 8; byte < 16; ++byte)
        assert(state.gpr[19].bytes[byte] == 0xBB);

    // EE DIV: signed quotient truncates toward zero; remainder follows dividend.
    writeLow64(state.gpr[1], 0xFFFFFFFFFFFFFFF9ull); // -7
    writeLow64(state.gpr[2], 3ull);
    auto divDecoded = decodeInstruction(encodeSpecial(1, 2, 0, 0, 0x1A));
    assert(executeSpecial(state, divDecoded) == ExecuteStatus::Executed);
    assert(readLow64(state.hilo.lo) == 0xFFFFFFFFFFFFFFFEull);
    assert(readLow64(state.hilo.hi) == 0xFFFFFFFFFFFFFFFFull);

    // EE DIVU uses unsigned 32-bit operands but sign-extends the 32-bit results.
    writeLow64(state.gpr[1], 0xFFFFFFFFFFFFFFFFull);
    writeLow64(state.gpr[2], 2ull);
    auto divuDecoded = decodeInstruction(encodeSpecial(1, 2, 0, 0, 0x1B));
    assert(executeSpecial(state, divuDecoded) == ExecuteStatus::Executed);
    assert(simd::readLane<std::uint64_t>(state.hilo.lo, 0) == 0x000000007FFFFFFFull);
    assert(simd::readLane<std::uint64_t>(state.hilo.hi, 0) == 1ull);

    // A zero divisor is architecturally undefined: reject atomically.
    writeLow64(state.gpr[2], 0ull);
    const auto divZeroPc = state.pc;
    const auto divZeroRetired = state.retiredInstructions;
    const auto divZeroLo = state.hilo.lo;
    const auto divZeroHi = state.hilo.hi;
    assert(executeSpecial(state, divuDecoded) == ExecuteStatus::Unsupported);
    assert(state.pc == divZeroPc && state.retiredInstructions == divZeroRetired);
    for (std::size_t byte = 0; byte < 16; ++byte)\n    {\n        assert(state.hilo.lo.bytes[byte] == divZeroLo.bytes[byte]);\n        assert(state.hilo.hi.bytes[byte] == divZeroHi.bytes[byte]);\n    }

    auto unsupported = decodeInstruction(encodeSpecial(1, 2, 7, 0, 0x20)); // ADD (trapping, intentionally unsupported)
    const auto pcBefore = state.pc;
    const auto retiredBefore = state.retiredInstructions;
    assert(executeSpecial(state, unsupported) == ExecuteStatus::Unsupported);
    assert(state.pc == pcBefore && state.retiredInstructions == retiredBefore);

    auto nonSpecial = decodeInstruction(0x8C220004u);
    assert(executeSpecial(state, nonSpecial) == ExecuteStatus::Unsupported);
    assert(state.pc == pcBefore && state.retiredInstructions == retiredBefore);
    return 0;
}
