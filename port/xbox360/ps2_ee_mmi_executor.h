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

    // PMFHL/PMTHL are top-level MMI functions (0x30/0x31), not MMI2/3.
    // The first transfer mode (sa=0) is the LW word-pair transfer.
    if (instruction.funct == 0x30u || instruction.funct == 0x31u)
    {
        if (instruction.sa != 0u)
            return MmiStatus::Unsupported;

        if (instruction.funct == 0x30u) // PMFHL.LW: LO0, HI0, LO2, HI2
        {
            auto &destination = state.gpr[instruction.rd & 31u];
            if ((instruction.rd & 31u) != 0u)
            {
                simd::writeLane<std::uint32_t>(destination, 0u,
                    simd::readLane<std::uint32_t>(state.hilo.lo, 0u));
                simd::writeLane<std::uint32_t>(destination, 1u,
                    simd::readLane<std::uint32_t>(state.hilo.hi, 0u));
                simd::writeLane<std::uint32_t>(destination, 2u,
                    simd::readLane<std::uint32_t>(state.hilo.lo, 2u));
                simd::writeLane<std::uint32_t>(destination, 3u,
                    simd::readLane<std::uint32_t>(state.hilo.hi, 2u));
            }
        }
        else // PMTHL.LW: partial write to LO/HI words 0 and 2
        {
            const auto &source = state.readGpr(instruction.rs);
            simd::writeLane<std::uint32_t>(state.hilo.lo, 0u,
                simd::readLane<std::uint32_t>(source, 0u));
            simd::writeLane<std::uint32_t>(state.hilo.hi, 0u,
                simd::readLane<std::uint32_t>(source, 1u));
            simd::writeLane<std::uint32_t>(state.hilo.lo, 2u,
                simd::readLane<std::uint32_t>(source, 2u));
            simd::writeLane<std::uint32_t>(state.hilo.hi, 2u,
                simd::readLane<std::uint32_t>(source, 3u));
        }

        state.advancePc();
        state.recordRetiredInstruction();
        return MmiStatus::Executed;
    }

    bool executed = false;
    if (instruction.mmiSubfamily == MmiSubfamily::Mmi0)
    {
        switch (instruction.sa)
        {
        case 0x00: // PADDW: wrapping packed 32-bit addition
        case 0x01: // PSUBW: wrapping packed 32-bit subtraction
        {
            const auto &lhs = state.readGpr(instruction.rs);
            const auto &rhs = state.readGpr(instruction.rt);
            simd::Vector128 value{};
            for (std::size_t lane = 0; lane < 4u; ++lane)
            {
                const auto a = simd::readLane<std::uint32_t>(lhs, lane);
                const auto b = simd::readLane<std::uint32_t>(rhs, lane);
                simd::writeLane<std::uint32_t>(value, lane,
                    instruction.sa == 0x00u ? a + b : a - b);
            }
            state.writeGpr(instruction.rd, value);
            executed = true;
            break;
        }
        default:
            break;
        }
    }
    else if (instruction.mmiSubfamily == MmiSubfamily::Mmi2)
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
        case 0x12: // PAND
            state.writeGpr(instruction.rd, simd::bitAnd(state.readGpr(instruction.rs),
                                                       state.readGpr(instruction.rt)));
            executed = true;
            break;
        case 0x13: // PXOR
            state.writeGpr(instruction.rd, simd::bitXor(state.readGpr(instruction.rs),
                                                       state.readGpr(instruction.rt)));
            executed = true;
            break;
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
        case 0x12: // POR
            state.writeGpr(instruction.rd, simd::bitOr(state.readGpr(instruction.rs),
                                                      state.readGpr(instruction.rt)));
            executed = true;
            break;
        case 0x13: // PNOR
            state.writeGpr(instruction.rd, simd::bitNor(state.readGpr(instruction.rs),
                                                       state.readGpr(instruction.rt)));
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
