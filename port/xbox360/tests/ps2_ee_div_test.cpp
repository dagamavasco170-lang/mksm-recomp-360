#include "ps2_ee_special_executor.h"

#include <cassert>
#include <cstdint>

using namespace ps2::xbox360::ee;
namespace simd = ps2::xbox360::simd;

static void writeLow64(simd::Vector128& value, std::uint64_t bits)
{
    for (std::size_t i = 0; i < 8; ++i)
        value.bytes[i] = static_cast<std::uint8_t>(bits >> (i * 8u));
}

static std::uint64_t readLow64(const simd::Vector128& value)
{
    std::uint64_t bits = 0;
    for (std::size_t i = 0; i < 8; ++i)
        bits |= static_cast<std::uint64_t>(value.bytes[i]) << (i * 8u);
    return bits;
}

static DecodedInstruction decodeDiv(std::uint8_t rs, std::uint8_t rt, bool isUnsigned)
{
    const std::uint32_t raw = (static_cast<std::uint32_t>(rs) << 21u) |
                              (static_cast<std::uint32_t>(rt) << 16u) |
                              (isUnsigned ? 0x1Bu : 0x1Au);
    return decodeInstruction(raw);
}

static void check(CpuState& state, std::uint8_t funct,
                  std::uint64_t lhs, std::uint64_t rhs,
                  std::uint64_t quotient, std::uint64_t remainder)
{
    writeLow64(state.gpr[1], lhs);
    writeLow64(state.gpr[2], rhs);
    const auto decoded = decodeDiv(1, 2, funct == 0x1B);
    const auto pc = state.pc;
    const auto retired = state.retiredInstructions;
    assert(decoded.special == (funct == 0x1B ? SpecialOperation::Divu : SpecialOperation::Div));
    assert(executeSpecial(state, decoded) == ExecuteStatus::Executed);
    assert(readLow64(state.hilo.lo) == quotient);
    assert(readLow64(state.hilo.hi) == remainder);
    assert(state.pc == pc + 4u);
    assert(state.retiredInstructions == retired + 1u);
}

int main()
{
    CpuState state{};
    state.pc = 0x4000;

    // Signed division truncates toward zero; remainder has dividend's sign.
    check(state, 0x1A, 0xFFFFFFFFFFFFFFF9ull, 3ull,
          0xFFFFFFFFFFFFFFFEull, 0xFFFFFFFFFFFFFFFFull);
    check(state, 0x1A, 0xFFFFFFFFFFFFFFF9ull, 0xFFFFFFFFFFFFFFFDull,
          2ull, 0xFFFFFFFFFFFFFFFFull);

    // Unsigned operands are 32-bit words; HI/LO words are sign-extended to 64.
    check(state, 0x1B, 0xFFFFFFFFFFFFFFFFull, 2ull,
          0x000000007FFFFFFFull, 1ull);
    check(state, 0x1B, 0xFFFFFFFF80000000ull, 3ull,
          0x000000002AAAAAAAull, 2ull);

    // INT32_MIN / -1 is handled without invoking C++ signed-overflow UB.
    check(state, 0x1A, 0xFFFFFFFF80000000ull, 0xFFFFFFFFFFFFFFFFull,
          0xFFFFFFFF80000000ull, 0ull);

    // A zero divisor is undefined for this EE operation: reject without mutation.
    writeLow64(state.gpr[1], 0xFFFFFFFFFFFFFFF9ull);
    writeLow64(state.gpr[2], 0ull);
    state.hilo.lo.bytes[0] = 0xA1;
    state.hilo.hi.bytes[0] = 0xB2;
    const auto pc = state.pc;
    const auto retired = state.retiredInstructions;
    const auto lo = state.hilo.lo;
    const auto hi = state.hilo.hi;
    const auto divu = decodeDiv(1, 2, true);
    assert(executeSpecial(state, divu) == ExecuteStatus::Unsupported);
    assert(state.pc == pc && state.retiredInstructions == retired);
    for (std::size_t i = 0; i < 16; ++i)
    {
        assert(state.hilo.lo.bytes[i] == lo.bytes[i]);
        assert(state.hilo.hi.bytes[i] == hi.bytes[i]);
    }

    // The executor rejects non-sign-extended scalar words rather than guessing.
    writeLow64(state.gpr[2], 3ull);
    writeLow64(state.gpr[1], 0x00000000FFFFFFFFull);
    const auto div = decodeDiv(1, 2, false);
    assert(executeSpecial(state, div) == ExecuteStatus::Unsupported);
    assert(state.pc == pc && state.retiredInstructions == retired);
    for (std::size_t i = 0; i < 16; ++i)
    {
        assert(state.hilo.lo.bytes[i] == lo.bytes[i]);
        assert(state.hilo.hi.bytes[i] == hi.bytes[i]);
    }
    return 0;
}
