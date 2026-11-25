#pragma once

#include "ps2_ee_cpu_state.h"
#include "ps2_ee_instruction_decode.h"

#include <cstdint>

namespace ps2::xbox360::ee
{
enum class ExecuteStatus : std::uint8_t { Executed, Unsupported };

// First conservative SPECIAL execution slice. It intentionally excludes control flow,
// memory, trapping arithmetic, and 64-bit operations until their EE semantics and
// delay-slot behavior are modeled explicitly.
[[nodiscard]] constexpr ExecuteStatus executeSpecial(CpuState &state,
                                                     const DecodedInstruction &d) noexcept
{
    if (d.kind != InstructionKind::Special)
        return ExecuteStatus::Unsupported;

    const auto &rs = state.readGpr(d.rs);
    const auto &rt = state.readGpr(d.rt);
    simd::Vector128 result{};
    bool writesRd = true;

    switch (d.special)
    {
    case SpecialOperation::And: result = simd::bitAnd(rs, rt); break;
    case SpecialOperation::Or: result = simd::bitOr(rs, rt); break;
    case SpecialOperation::Xor: result = simd::bitXor(rs, rt); break;
    case SpecialOperation::Nor: result = simd::bitNor(rs, rt); break;
    case SpecialOperation::Movz:
        if (simd::readLane<std::uint32_t>(rt, 0) != 0u) writesRd = false;
        else result = rs;
        break;
    case SpecialOperation::Movn:
        if (simd::readLane<std::uint32_t>(rt, 0) == 0u) writesRd = false;
        else result = rs;
        break;
    case SpecialOperation::Mfhi:
        result = state.gpr[d.rd];
        for (std::size_t byte = 0; byte < 8; ++byte)
            result.bytes[byte] = state.hilo.hi.bytes[byte];
        break;
    case SpecialOperation::Mflo:
        result = state.gpr[d.rd];
        for (std::size_t byte = 0; byte < 8; ++byte)
            result.bytes[byte] = state.hilo.lo.bytes[byte];
        break;
    case SpecialOperation::Mthi:
        for (std::size_t byte = 0; byte < 8; ++byte)
            state.hilo.hi.bytes[byte] = rs.bytes[byte];
        writesRd = false;
        break;
    case SpecialOperation::Mtlo:
        for (std::size_t byte = 0; byte < 8; ++byte)
            state.hilo.lo.bytes[byte] = rs.bytes[byte];
        writesRd = false;
        break;
    case SpecialOperation::Sll:
        simd::writeLane<std::uint32_t>(result, 0, simd::readLane<std::uint32_t>(rt, 0) << d.sa);
        break;
    case SpecialOperation::Srl:
        simd::writeLane<std::uint32_t>(result, 0, simd::readLane<std::uint32_t>(rt, 0) >> d.sa);
        break;
    case SpecialOperation::Sllv:
        simd::writeLane<std::uint32_t>(result, 0,
            simd::readLane<std::uint32_t>(rt, 0) <<
            (simd::readLane<std::uint32_t>(rs, 0) & 0x1Fu));
        break;
    case SpecialOperation::Srlv:
        simd::writeLane<std::uint32_t>(result, 0,
            simd::readLane<std::uint32_t>(rt, 0) >>
            (simd::readLane<std::uint32_t>(rs, 0) & 0x1Fu));
        break;
    case SpecialOperation::Srav:
    {
        const auto value = simd::detail::asSigned(simd::readLane<std::uint32_t>(rt, 0));
        const auto shift = simd::readLane<std::uint32_t>(rs, 0) & 0x1Fu;
        simd::writeLane<std::uint32_t>(result, 0,
            static_cast<std::uint32_t>(static_cast<std::int32_t>(value >> shift)));
        break;
    }
    case SpecialOperation::Addu:
        simd::writeLane<std::uint32_t>(result, 0,
            simd::readLane<std::uint32_t>(rs, 0) + simd::readLane<std::uint32_t>(rt, 0));
        break;
    case SpecialOperation::Subu:
        simd::writeLane<std::uint32_t>(result, 0,
            simd::readLane<std::uint32_t>(rs, 0) - simd::readLane<std::uint32_t>(rt, 0));
        break;
    case SpecialOperation::Slt:
        simd::writeLane<std::uint32_t>(result, 0,
            simd::detail::asSigned(simd::readLane<std::uint32_t>(rs, 0)) <
            simd::detail::asSigned(simd::readLane<std::uint32_t>(rt, 0)) ? 1u : 0u);
        break;
    case SpecialOperation::Sltu:
        simd::writeLane<std::uint32_t>(result, 0,
            simd::readLane<std::uint32_t>(rs, 0) < simd::readLane<std::uint32_t>(rt, 0) ? 1u : 0u);
        break;
    case SpecialOperation::Sra:
    {
        const auto value = simd::detail::asSigned(simd::readLane<std::uint32_t>(rt, 0));
        const auto shifted = static_cast<std::int32_t>(value >> d.sa);
        simd::writeLane<std::uint32_t>(result, 0, static_cast<std::uint32_t>(shifted));
        break;
    }
    default:
        return ExecuteStatus::Unsupported;
    }

    if (writesRd)
        state.writeGpr(d.rd, result);
    state.advancePc();
    state.recordRetiredInstruction();
    return ExecuteStatus::Executed;
}
} // namespace ps2::xbox360::ee
