#pragma once

#include "ps2_ee_instruction_decode.h"
#include "ps2_ee_immediate_executor.h"
#include "ps2_ee_load_store_executor.h"
#include "ps2_ee_special_executor.h"
#include "ps2_guest_memory.h"

#include <cstdint>
#include <span>

namespace ps2::xbox360::ee
{
enum class StepStatus : std::uint8_t { Executed, Unsupported, InvalidMemory, AddressError };

struct StepResult
{
    StepStatus status{StepStatus::Unsupported};
    std::uint32_t instruction{};
    DecodedInstruction decoded{};
};

// Executes one instruction from the current PC. The existing instruction executors
// own PC advancement and retirement. Control-flow and delay-slot instructions are
// intentionally rejected until the pipeline is integrated into this dispatcher.
[[nodiscard]] inline StepResult step(CpuState &state, std::span<std::uint8_t> memory) noexcept
{
    std::uint32_t word{};
    if (!guest_memory::readLittleEndian(std::span<const std::uint8_t>(memory), state.pc, word))
        return {StepStatus::InvalidMemory, 0u, {}};

    const auto decoded = decodeInstruction(word);
    if (decoded.kind == InstructionKind::Special)
    {
        if (decoded.special == SpecialOperation::Jr || decoded.special == SpecialOperation::Jalr)
            return {StepStatus::Unsupported, word, decoded};
        const auto status = executeSpecial(state, decoded);
        return {status == ExecuteStatus::Executed ? StepStatus::Executed : StepStatus::Unsupported,
                word, decoded};
    }

    if (decoded.opcode == 0x09u || decoded.opcode == 0x0Cu ||
        decoded.opcode == 0x0Du || decoded.opcode == 0x0Eu || decoded.opcode == 0x0Fu)
    {
        const auto status = executeImmediate(state, decoded);
        return {status == ImmediateStatus::Executed ? StepStatus::Executed : StepStatus::Unsupported,
                word, decoded};
    }

    if (decoded.kind == InstructionKind::LoadStore)
    {
        const auto status = executeLoadStore(state, decoded, memory);
        switch (status)
        {
        case LoadStoreStatus::Executed: return {StepStatus::Executed, word, decoded};
        case LoadStoreStatus::AddressError: return {StepStatus::AddressError, word, decoded};
        case LoadStoreStatus::InvalidMemory: return {StepStatus::InvalidMemory, word, decoded};
        case LoadStoreStatus::Unsupported: return {StepStatus::Unsupported, word, decoded};
        }
    }
    return {StepStatus::Unsupported, word, decoded};
}
} // namespace ps2::xbox360::ee
