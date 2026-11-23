#include "ps2_ee_special_executor.h"

#include <cassert>
#include <cstdint>

using namespace ps2::xbox360::ee;

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
    CpuState snapshot{};
    state.pc = 0x1000;
    simd::writeLane<std::uint32_t>(state.gpr[1], 0, 0x12345678u);
    simd::writeLane<std::uint32_t>(state.gpr[2], 0, 0x00FF00FFu);

    auto andDecoded = decodeInstruction(encodeSpecial(1, 2, 3, 0, 0x24));
    assert(executeSpecial(snapshot, state, andDecoded) == ExecuteStatus::Executed);
    assert(simd::readLane<std::uint32_t>(state.gpr[3], 0) == 0x00340078u);
    assert(state.pc == 0x1004 && state.retiredInstructions == 1);

    auto norDecoded = decodeInstruction(encodeSpecial(1, 2, 4, 0, 0x27));
    assert(executeSpecial(snapshot, state, norDecoded) == ExecuteStatus::Executed);
    assert(simd::readLane<std::uint32_t>(state.gpr[4], 0) == 0xED00ED00u);

    auto movzDecoded = decodeInstruction(encodeSpecial(1, 0, 5, 0, 0x0A));
    assert(executeSpecial(snapshot, state, movzDecoded) == ExecuteStatus::Executed);
    assert(simd::readLane<std::uint32_t>(state.gpr[5], 0) == 0x12345678u);

    simd::writeLane<std::uint32_t>(state.gpr[6], 0, 0xDEADBEEFu);
    auto movnDecoded = decodeInstruction(encodeSpecial(1, 2, 6, 0, 0x0B));
    assert(executeSpecial(snapshot, state, movnDecoded) == ExecuteStatus::Executed);
    assert(simd::readLane<std::uint32_t>(state.gpr[6], 0) == 0x12345678u);

    auto unsupported = decodeInstruction(encodeSpecial(1, 2, 7, 0, 0x21)); // ADDU
    const auto pcBefore = state.pc;
    const auto retiredBefore = state.retiredInstructions;
    assert(executeSpecial(snapshot, state, unsupported) == ExecuteStatus::Unsupported);
    assert(state.pc == pcBefore && state.retiredInstructions == retiredBefore);

    auto nonSpecial = decodeInstruction(0x8C220004u);
    assert(executeSpecial(snapshot, state, nonSpecial) == ExecuteStatus::Unsupported);
    assert(state.pc == pcBefore && state.retiredInstructions == retiredBefore);
    return 0;
}
