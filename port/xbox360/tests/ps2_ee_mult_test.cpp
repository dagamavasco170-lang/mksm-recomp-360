#include "ps2_ee_special_executor.h"

#include <cassert>
#include <cstdint>

using namespace ps2::xbox360::ee;
namespace simd = ps2::xbox360::simd;

static constexpr std::uint32_t encode(std::uint8_t rs, std::uint8_t rt,
                                      std::uint8_t rd, std::uint8_t funct)
{
    return (static_cast<std::uint32_t>(rs) << 21) |
           (static_cast<std::uint32_t>(rt) << 16) |
           (static_cast<std::uint32_t>(rd) << 11) | funct;
}

static void runCase(std::uint8_t funct, std::uint32_t lhs, std::uint32_t rhs,
                    std::uint64_t expectedLo, std::uint64_t expectedHi)
{
    CpuState state{};
    state.pc = 0x2000;
    state.retiredInstructions = 4;
    simd::writeLane<std::uint32_t>(state.gpr[1], 0, lhs);
    simd::writeLane<std::uint32_t>(state.gpr[2], 0, rhs);
    for (std::size_t i = 8; i < 16; ++i)
    {
        state.gpr[3].bytes[i] = 0xA5;
        state.hilo.lo.bytes[i] = 0xB6;
        state.hilo.hi.bytes[i] = 0xC7;
    }

    const auto decoded = decodeInstruction(encode(1, 2, 3, funct));
    assert(decoded.special == (funct == 0x18 ? SpecialOperation::Mult
                                             : SpecialOperation::Multu));
    assert(executeSpecial(state, decoded) == ExecuteStatus::Executed);
    assert(state.pc == 0x2004);
    assert(state.retiredInstructions == 5);
    for (std::size_t byte = 0; byte < 8; ++byte)
    {
        assert(state.hilo.lo.bytes[byte] ==
               static_cast<std::uint8_t>(expectedLo >> (byte * 8u)));
        assert(state.hilo.hi.bytes[byte] ==
               static_cast<std::uint8_t>(expectedHi >> (byte * 8u)));
        assert(state.gpr[3].bytes[byte] == state.hilo.lo.bytes[byte]);
    }
    for (std::size_t byte = 8; byte < 16; ++byte)
    {
        assert(state.gpr[3].bytes[byte] == 0xA5);
        assert(state.hilo.lo.bytes[byte] == 0xB6);
        assert(state.hilo.hi.bytes[byte] == 0xC7);
    }
}

int main()
{
    // -3 * 7 = -21; both HI/LO 32-bit words are sign-extended to 64 bits.
    runCase(0x18, 0xFFFFFFFDu, 7u, 0xFFFFFFFFFFFFFFEBull,
            0xFFFFFFFFFFFFFFFFull);

    // Unsigned product is 0x00000001FFFFFFFE; LO's low word is sign-extended.
    runCase(0x19, 0xFFFFFFFFu, 2u, 0xFFFFFFFFFFFFFFFEull, 1u);
    return 0;
}
