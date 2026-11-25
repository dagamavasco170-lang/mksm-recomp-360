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
    case SpecialOperation::Mult:
    case SpecialOperation::Multu:
    {
        const std::uint32_t lhsBits = simd::readLane<std::uint32_t>(rs, 0);
        const std::uint32_t rhsBits = simd::readLane<std::uint32_t>(rt, 0);
        std::uint64_t product = 0;
        if (d.special == SpecialOperation::Mult)
        {
            const std::int64_t lhs = simd::detail::asSigned(lhsBits);
            const std::int64_t rhs = simd::detail::asSigned(rhsBits);
            product = static_cast<std::uint64_t>(lhs * rhs);
        }
        else
        {
            product = static_cast<std::uint64_t>(lhsBits) * rhsBits;
        }

        const auto signExtendedWord = [](std::uint32_t word) constexpr {
            const std::int32_t signedWord = simd::detail::asSigned(word);
            return static_cast<std::uint64_t>(static_cast<std::int64_t>(signedWord));
        };
        const std::uint32_t lowWord = static_cast<std::uint32_t>(product);
        const std::uint32_t highWord = static_cast<std::uint32_t>(product >> 32u);
        const std::uint64_t loValue = signExtendedWord(lowWord);
        const std::uint64_t hiValue = signExtendedWord(highWord);

        for (std::size_t byte = 0; byte < 8u; ++byte)
        {
            state.hilo.lo.bytes[byte] = static_cast<std::uint8_t>(loValue >> (byte * 8u));
            state.hilo.hi.bytes[byte] = static_cast<std::uint8_t>(hiValue >> (byte * 8u));
        }
        // R5900 MULT/MULTU also expose the low product word through rd.
        // Only GPR[rd] bits 63..0 are written; its upper 64 bits are preserved.
        result = state.gpr[d.rd];
        for (std::size_t byte = 0; byte < 8u; ++byte)
            result.bytes[byte] = static_cast<std::uint8_t>(loValue >> (byte * 8u));
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
