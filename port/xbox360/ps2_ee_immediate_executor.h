#pragma once

#include "ps2_ee_cpu_state.h"
#include "ps2_ee_instruction_decode.h"

#include <cstdint>
#include <bit>

namespace ps2::xbox360::ee
{
enum class ImmediateStatus : std::uint8_t { Executed, Unsupported };

[[nodiscard]] constexpr ImmediateStatus executeImmediate(CpuState &state,
                                                        const DecodedInstruction &d) noexcept
{
    simd::Vector128 value{};
    const std::uint32_t rs = simd::readLane<std::uint32_t>(state.readGpr(d.rs), 0);
    const std::uint32_t imm = d.immediate;
    switch (d.opcode)
    {
    case 0x09: // ADDIU: modulo-32-bit addition, no overflow trap
        simd::writeLane<std::uint32_t>(value, 0, rs + static_cast<std::uint32_t>(
            static_cast<std::int32_t>(std::bit_cast<std::int16_t>(d.immediate))));
        break;
    case 0x0C: // ANDI
        simd::writeLane<std::uint32_t>(value, 0, rs & imm);
        break;
    case 0x0D: // ORI
        simd::writeLane<std::uint32_t>(value, 0, rs | imm);
        break;
    case 0x0E: // XORI
        simd::writeLane<std::uint32_t>(value, 0, rs ^ imm);
        break;
    case 0x0F: // LUI
        simd::writeLane<std::uint32_t>(value, 0, imm << 16u);
        break;
    default:
        return ImmediateStatus::Unsupported;
    }
    state.writeGpr(d.rt, value);
    state.advancePc();
    state.recordRetiredInstruction();
    return ImmediateStatus::Executed;
}
} // namespace ps2::xbox360::ee
