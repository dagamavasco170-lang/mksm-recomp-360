#pragma once

#include "ps2_ee_cpu_state.h"
#include "ps2_ee_instruction_decode.h"

#include <cstdint>

namespace ps2::xbox360::ee
{
enum class MmiStatus : std::uint8_t
{
    Executed,
    Unsupported
};

// First dispatcher-connected MMI slice. Keep unsupported encodings explicit:
// this is not a complete R5900 MMI implementation.
[[nodiscard]] inline MmiStatus executeMmi(CpuState &state,
                                          const DecodedInstruction &instruction) noexcept
{
    if (instruction.kind != InstructionKind::Mmi)
        return MmiStatus::Unsupported;

    bool executed = false;
    if (instruction.mmiSubfamily == MmiSubfamily::Mmi2)
    {
        switch (instruction.sa)
        {
        case 0x08: // PMFHI
            state.pmfhi(instruction.rd);
            executed = true;
            break;
        case 0x09: // PMFLO
            state.pmflo(instruction.rd);
            executed = true;
            break;
        case 0x10: // PMADDH
        {
            const auto result = simd::pmaddh(state.readGpr(instruction.rs),
                                             state.readGpr(instruction.rt),
                                             state.hilo.hi, state.hilo.lo);
            state.hilo.hi = result.hi;
            state.hilo.lo = result.lo;
            state.writeGpr(instruction.rd, result.destination);
            executed = true;
            break;
        }
        case 0x14: // PMSUBH
        {
            const auto result = simd::pmsubh(state.readGpr(instruction.rs),
                                             state.readGpr(instruction.rt),
                                             state.hilo.hi, state.hilo.lo);
            state.hilo.hi = result.hi;
            state.hilo.lo = result.lo;
            state.writeGpr(instruction.rd, result.destination);
            executed = true;
            break;
        }
        default:
            break;
        }
    }
    else if (instruction.mmiSubfamily == MmiSubfamily::Mmi3)
    {
        switch (instruction.sa)
        {
        case 0x08: // PMTHI
            state.pmthi(instruction.rs);
            executed = true;
            break;
        case 0x09: // PMTLO
            state.pmtlo(instruction.rs);
            executed = true;
            break;
        default:
            break;
        }
    }

    if (!executed)
        return MmiStatus::Unsupported;

    state.advancePc();
    state.recordRetiredInstruction();
    return MmiStatus::Executed;
}
} // namespace ps2::xbox360::ee
