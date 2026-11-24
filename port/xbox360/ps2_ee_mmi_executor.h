#pragma once

#include "ps2_ee_cpu_state.h"
#include "ps2_ee_instruction_decode.h"

#include <bit>
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
        case 0x04: // PADDH: wrapping packed 16-bit addition
        case 0x05: // PSUBH: wrapping packed 16-bit subtraction
        {
            const auto &lhs = state.readGpr(instruction.rs);
            const auto &rhs = state.readGpr(instruction.rt);
            simd::Vector128 value{};
            for (std::size_t lane = 0; lane < 8u; ++lane)
            {
                const auto a = simd::readLane<std::uint16_t>(lhs, lane);
                const auto b = simd::readLane<std::uint16_t>(rhs, lane);
                const auto result = instruction.sa == 0x04u
                    ? static_cast<std::uint16_t>(a + b)
                    : static_cast<std::uint16_t>(a - b);
                simd::writeLane<std::uint16_t>(value, lane, result);
            }
            state.writeGpr(instruction.rd, value);
            executed = true;
            break;
        }
        case 0x06: // PCGTH: signed packed halfword greater-than
        case 0x07: // PMAXH: signed packed halfword maximum
        {
            const auto &lhs = state.readGpr(instruction.rs);
            const auto &rhs = state.readGpr(instruction.rt);
            simd::Vector128 value{};
            for (std::size_t lane = 0; lane < 8u; ++lane)
            {
                const auto a = simd::readLane<std::uint16_t>(lhs, lane);
                const auto b = simd::readLane<std::uint16_t>(rhs, lane);
                const auto signedA = std::bit_cast<std::int16_t>(a);
                const auto signedB = std::bit_cast<std::int16_t>(b);
                const auto result = instruction.sa == 0x06u
                    ? (signedA > signedB ? 0xFFFFu : 0u)
                    : (signedA > signedB ? a : b);
                simd::writeLane<std::uint16_t>(value, lane, static_cast<std::uint16_t>(result));
            }
            state.writeGpr(instruction.rd, value);
            executed = true;
            break;
        }
        case 0x08: // PADDB: wrapping packed byte addition
        case 0x09: // PSUBB: wrapping packed byte subtraction
        {
            const auto &lhs = state.readGpr(instruction.rs);
            const auto &rhs = state.readGpr(instruction.rt);
            simd::Vector128 value{};
            for (std::size_t lane = 0; lane < 16u; ++lane)
            {
                const auto a = lhs.bytes[lane];
                const auto b = rhs.bytes[lane];
                value.bytes[lane] = instruction.sa == 0x08u
                    ? static_cast<std::uint8_t>(a + b)
                    : static_cast<std::uint8_t>(a - b);
            }
            state.writeGpr(instruction.rd, value);
            executed = true;
            break;
        }
        case 0x0A: // PCGTB: signed packed byte greater-than
        {
            const auto &lhs = state.readGpr(instruction.rs);
            const auto &rhs = state.readGpr(instruction.rt);
            simd::Vector128 value{};
            for (std::size_t lane = 0; lane < 16u; ++lane)
                value.bytes[lane] = std::bit_cast<std::int8_t>(lhs.bytes[lane]) >
                                   std::bit_cast<std::int8_t>(rhs.bytes[lane]) ? 0xFFu : 0u;
            state.writeGpr(instruction.rd, value);
            executed = true;
            break;
        }
        case 0x00: // PADDW: wrapping packed 32-bit addition
        case 0x01: // PSUBW: wrapping packed 32-bit subtraction
        case 0x02: // PCGTW: signed packed greater-than word
        case 0x03: // PMAXW: signed packed maximum word
        case 0x10: // PADDSW: signed saturating packed word addition
        case 0x11: // PSUBSW: signed saturating packed word subtraction
        {
            const auto &lhs = state.readGpr(instruction.rs);
            const auto &rhs = state.readGpr(instruction.rt);
            simd::Vector128 value{};
            for (std::size_t lane = 0; lane < 4u; ++lane)
            {
                const auto a = simd::readLane<std::uint32_t>(lhs, lane);
                const auto b = simd::readLane<std::uint32_t>(rhs, lane);
                std::uint32_t result{};
                switch (instruction.sa)
                {
                case 0x00: result = a + b; break;
                case 0x01: result = a - b; break;
                case 0x02:
                    result = std::bit_cast<std::int32_t>(a) >
                             std::bit_cast<std::int32_t>(b) ? 0xFFFFFFFFu : 0u;
                    break;
                case 0x03:
                    result = std::bit_cast<std::int32_t>(a) >
                             std::bit_cast<std::int32_t>(b) ? a : b;
                    break;
                case 0x10:
                case 0x11:
                {
                    const auto signedA = std::bit_cast<std::int32_t>(a);
                    const auto signedB = std::bit_cast<std::int32_t>(b);
                    const std::int64_t wide = instruction.sa == 0x10u
                        ? static_cast<std::int64_t>(signedA) + signedB
                        : static_cast<std::int64_t>(signedA) - signedB;
                    const auto clamped = wide > INT32_MAX ? INT32_MAX
                        : (wide < INT32_MIN ? INT32_MIN : static_cast<std::int32_t>(wide));
                    result = std::bit_cast<std::uint32_t>(clamped);
                    break;
                }
                default: break;
                }
                simd::writeLane<std::uint32_t>(value, lane, result);
            }
            state.writeGpr(instruction.rd, value);
            executed = true;
            break;
        }
        default:
            break;
        }
    }
    else if (instruction.mmiSubfamily == MmiSubfamily::Mmi1)
    {
        switch (instruction.sa)
        {
        case 0x01: // PABSW: saturating absolute signed packed words
        {
            const auto &source = state.readGpr(instruction.rt);
            simd::Vector128 value{};
            for (std::size_t lane = 0; lane < 4u; ++lane)
            {
                const auto bits = simd::readLane<std::uint32_t>(source, lane);
                const auto signedValue = std::bit_cast<std::int32_t>(bits);
                const auto absolute = signedValue == INT32_MIN ? INT32_MAX
                    : (signedValue < 0 ? -signedValue : signedValue);
                simd::writeLane<std::uint32_t>(value, lane,
                    std::bit_cast<std::uint32_t>(absolute));
            }
            state.writeGpr(instruction.rd, value);
            executed = true;
            break;
        }
        case 0x05: // PABSH: absolute signed packed halfwords
        {
            const auto &source = state.readGpr(instruction.rs);
            simd::Vector128 value{};
            for (std::size_t lane = 0; lane < 8u; ++lane)
            {
                const auto bits = simd::readLane<std::uint16_t>(source, lane);
                const auto signedValue = std::bit_cast<std::int16_t>(bits);
                const auto absolute = signedValue < 0
                    ? static_cast<std::uint16_t>(-static_cast<std::int32_t>(signedValue))
                    : bits;
                simd::writeLane<std::uint16_t>(value, lane, absolute);
            }
            state.writeGpr(instruction.rd, value);
            executed = true;
            break;
        }
        case 0x0A: // PCEQB: packed byte equality
        {
            const auto &lhs = state.readGpr(instruction.rs);
            const auto &rhs = state.readGpr(instruction.rt);
            simd::Vector128 value{};
            for (std::size_t lane = 0; lane < 16u; ++lane)
            {
                const auto a = lhs.bytes[lane];
                const auto b = rhs.bytes[lane];
                value.bytes[lane] = a == b ? 0xFFu : 0u;
            }
            state.writeGpr(instruction.rd, value);
            executed = true;
            break;
        }
        case 0x06: // PCEQH: packed 16-bit equality
        case 0x07: // PMINH: signed packed 16-bit minimum
        {
            const auto &lhs = state.readGpr(instruction.rs);
            const auto &rhs = state.readGpr(instruction.rt);
            simd::Vector128 value{};
            for (std::size_t lane = 0; lane < 8u; ++lane)
            {
                const auto a = simd::readLane<std::uint16_t>(lhs, lane);
                const auto b = simd::readLane<std::uint16_t>(rhs, lane);
                const std::uint16_t result = instruction.sa == 0x06u
                    ? (a == b ? 0xFFFFu : 0u)
                    : (std::bit_cast<std::int16_t>(a) <
                       std::bit_cast<std::int16_t>(b) ? a : b);
                simd::writeLane<std::uint16_t>(value, lane, result);
            }
            state.writeGpr(instruction.rd, value);
            executed = true;
            break;
        }
        case 0x10: // PADDUW: saturating unsigned packed word addition
        case 0x11: // PSUBUW: saturating unsigned packed word subtraction
        {
            const auto &lhs = state.readGpr(instruction.rs);
            const auto &rhs = state.readGpr(instruction.rt);
            simd::Vector128 value{};
            for (std::size_t lane = 0; lane < 4u; ++lane)
            {
                const auto a = simd::readLane<std::uint32_t>(lhs, lane);
                const auto b = simd::readLane<std::uint32_t>(rhs, lane);
                const auto result = instruction.sa == 0x10u
                    ? (static_cast<std::uint64_t>(a) + b > UINT32_MAX
                        ? UINT32_MAX : static_cast<std::uint32_t>(a + b))
                    : (a < b ? 0u : a - b);
                simd::writeLane<std::uint32_t>(value, lane, result);
            }
            state.writeGpr(instruction.rd, value);
            executed = true;
            break;
        }
        case 0x14: // PADDUH: saturating unsigned packed halfword addition
        case 0x15: // PSUBUH: saturating unsigned packed halfword subtraction
        {
            const auto &lhs = state.readGpr(instruction.rs);
            const auto &rhs = state.readGpr(instruction.rt);
            simd::Vector128 value{};
            for (std::size_t lane = 0; lane < 8u; ++lane)
            {
                const auto a = simd::readLane<std::uint16_t>(lhs, lane);
                const auto b = simd::readLane<std::uint16_t>(rhs, lane);
                const auto result = instruction.sa == 0x14u
                    ? (static_cast<std::uint32_t>(a) + b > UINT16_MAX
                        ? UINT16_MAX : static_cast<std::uint16_t>(a + b))
                    : (a < b ? 0u : static_cast<std::uint16_t>(a - b));
                simd::writeLane<std::uint16_t>(value, lane, result);
            }
            state.writeGpr(instruction.rd, value);
            executed = true;
            break;
        }
        case 0x18: // PADDUB: saturating unsigned packed byte addition
        case 0x19: // PSUBUB: saturating unsigned packed byte subtraction
        {
            const auto &lhs = state.readGpr(instruction.rs);
            const auto &rhs = state.readGpr(instruction.rt);
            simd::Vector128 value{};
            for (std::size_t lane = 0; lane < 16u; ++lane)
            {
                const auto a = lhs.bytes[lane];
                const auto b = rhs.bytes[lane];
                value.bytes[lane] = instruction.sa == 0x18u
                    ? (static_cast<unsigned>(a) + b > UINT8_MAX
                        ? UINT8_MAX : static_cast<std::uint8_t>(a + b))
                    : (a < b ? 0u : static_cast<std::uint8_t>(a - b));
            }
            state.writeGpr(instruction.rd, value);
            executed = true;
            break;
        }
        case 0x02: // PCEQW: packed 32-bit equality
        case 0x03: // PMINW: signed packed 32-bit minimum
        {
            const auto &lhs = state.readGpr(instruction.rs);
            const auto &rhs = state.readGpr(instruction.rt);
            simd::Vector128 value{};
            for (std::size_t lane = 0; lane < 4u; ++lane)
            {
                const auto a = simd::readLane<std::uint32_t>(lhs, lane);
                const auto b = simd::readLane<std::uint32_t>(rhs, lane);
                const std::uint32_t result = instruction.sa == 0x02u
                    ? (a == b ? 0xFFFFFFFFu : 0u)
                    : (std::bit_cast<std::int32_t>(a) <
                       std::bit_cast<std::int32_t>(b) ? a : b);
                simd::writeLane<std::uint32_t>(value, lane, result);
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
