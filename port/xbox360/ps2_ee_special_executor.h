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
[[nodiscard]] constexpr ExecuteStatus executeSpecial(const CpuState &input,
                                                     CpuState &state,
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
    case SpecialOperation::Sll:
        simd::writeLane<std::uint32_t>(result, 0, simd::readLane<std::uint32_t>(rt, 0) << d.sa);
        break;
    case SpecialOperation::Srl:
        simd::writeLane<std::uint32_t>(result, 0, simd::readLane<std::uint32_t>(rt, 0) >> d.sa);
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
    (void)input; // Kept in the signature for future transactional execution/error handling.
    return ExecuteStatus::Executed;
}
} // namespace ps2::xbox360::ee
