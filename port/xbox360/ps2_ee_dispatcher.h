#pragma once
#include "ps2_ee_instruction_decode.h"
#include "ps2_ee_immediate_executor.h"
#include "ps2_ee_load_store_executor.h"
#include "ps2_ee_mmi_executor.h"
#include "ps2_ee_special_executor.h"
#include "ps2_guest_memory.h"
#include <bit>
#include <cstdint>
#include <span>

namespace ps2::xbox360::ee
{
enum class StepStatus : std::uint8_t { Executed, Unsupported, InvalidMemory, AddressError };
struct StepResult { StepStatus status{StepStatus::Unsupported}; std::uint32_t instruction{}; DecodedInstruction decoded{}; };

[[nodiscard]] constexpr std::uint32_t branchTarget(std::uint32_t pc, std::uint16_t imm) noexcept
{
    const auto offset = static_cast<std::int32_t>(std::bit_cast<std::int16_t>(imm));
    return pc + 4u + static_cast<std::uint32_t>(offset * 4);
}
[[nodiscard]] constexpr simd::Vector128 scalarVector(std::uint32_t value) noexcept
{
    simd::Vector128 v{};
    simd::writeLane<std::uint32_t>(v, 0, value);
    return v;
}

[[nodiscard]] inline StepResult step(CpuState &state, std::span<std::uint8_t> memory) noexcept
{
    const bool wasDelaySlot = state.branchDelayPending;
    std::uint32_t word{};
    if (!guest_memory::readLittleEndian(std::span<const std::uint8_t>(memory), state.pc, word))
        return {StepStatus::InvalidMemory, 0u, {}};
    const auto d = decodeInstruction(word);
    const auto rs = simd::readLane<std::uint32_t>(state.readGpr(d.rs), 0);
    const auto rt = simd::readLane<std::uint32_t>(state.readGpr(d.rt), 0);

    // Branch-in-delay behavior is EE-specific and not yet modeled; fail atomically.
    if (wasDelaySlot && (d.kind == InstructionKind::BranchImmediate ||
        (d.kind == InstructionKind::Special &&
         (d.special == SpecialOperation::Jr || d.special == SpecialOperation::Jalr))))
        return {StepStatus::Unsupported, word, d};

    if (d.kind == InstructionKind::BranchImmediate)
    {
        bool taken = false, link = false;
        switch (d.opcode)
        {
        case 0x01:
            switch (d.rt)
            {
            case 0x00: taken = std::bit_cast<std::int32_t>(rs) < 0; break; // BLTZ
            case 0x01: taken = std::bit_cast<std::int32_t>(rs) >= 0; break; // BGEZ
            case 0x10: taken = std::bit_cast<std::int32_t>(rs) < 0; link = true; break; // BLTZAL
            case 0x11: taken = std::bit_cast<std::int32_t>(rs) >= 0; link = true; break; // BGEZAL
            default: return {StepStatus::Unsupported, word, d};
            }
            break;
        case 0x02: case 0x03: // J/JAL
            if (d.opcode == 0x03) state.writeGpr(31, scalarVector(state.pc + 8u));
            state.scheduleBranch(((state.pc + 4u) & 0xF0000000u) | (d.target << 2u));
            state.advancePc(); state.recordRetiredInstruction();
            return {StepStatus::Executed, word, d};
        case 0x04: taken = rs == rt; break; // BEQ
        case 0x05: taken = rs != rt; break; // BNE
        case 0x06: taken = std::bit_cast<std::int32_t>(rs) <= 0; break; // BLEZ
        case 0x07: taken = std::bit_cast<std::int32_t>(rs) > 0; break; // BGTZ
        default: return {StepStatus::Unsupported, word, d};
        }
        if (link && taken) state.writeGpr(31, scalarVector(state.pc + 8u));
        state.advancePc(); state.recordRetiredInstruction();
        state.scheduleBranch(taken ? branchTarget(state.pc - 4u, d.immediate) : state.pc + 4u);
        return {StepStatus::Executed, word, d};
    }
    if (d.kind == InstructionKind::Special &&
        (d.special == SpecialOperation::Jr || d.special == SpecialOperation::Jalr))
    {
        const auto target = rs;
        if (d.special == SpecialOperation::Jalr) state.writeGpr(d.rd, scalarVector(state.pc + 8u));
        state.advancePc(); state.recordRetiredInstruction(); state.scheduleBranch(target);
        return {StepStatus::Executed, word, d};
    }

    StepStatus status = StepStatus::Unsupported;
    if (d.kind == InstructionKind::Special)
    {
        status = executeSpecial(state, d) == ExecuteStatus::Executed ? StepStatus::Executed : StepStatus::Unsupported;
    }
    else if (d.opcode == 0x09u || d.opcode == 0x0Cu || d.opcode == 0x0Du || d.opcode == 0x0Eu || d.opcode == 0x0Fu)
    {
        status = executeImmediate(state, d) == ImmediateStatus::Executed ? StepStatus::Executed : StepStatus::Unsupported;
    }
    else if (d.kind == InstructionKind::Mmi)
    {
        status = executeMmi(state, d) == MmiStatus::Executed ? StepStatus::Executed : StepStatus::Unsupported;
    }
    else if (d.kind == InstructionKind::LoadStore)
    {
        switch (executeLoadStore(state, d, memory))
        {
        case LoadStoreStatus::Executed: status = StepStatus::Executed; break;
        case LoadStoreStatus::AddressError: status = StepStatus::AddressError; break;
        case LoadStoreStatus::InvalidMemory: status = StepStatus::InvalidMemory; break;
        case LoadStoreStatus::Unsupported: status = StepStatus::Unsupported; break;
        }
    }
    if (status == StepStatus::Executed) state.finishDelaySlot(wasDelaySlot);
    return {status, word, d};
}
} // namespace ps2::xbox360::ee
