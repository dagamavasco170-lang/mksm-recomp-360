#include "ps2_ee_cpu_state.h"

#include <cstdint>

namespace ps2::xbox360::ee
{
// Minimal control-flow bookkeeping for MIPS branch delay slots.
// Branch executors compute the target and call scheduleBranch().
// The dispatcher captures inDelaySlot() before executing each instruction,
// then calls finishInstruction(wasDelaySlot) after successful retirement.
struct BranchDelayState
{
    bool delaySlotPending{};
    std::uint32_t branchTarget{};

    [[nodiscard]] constexpr bool inDelaySlot() const noexcept
    {
        return delaySlotPending;
    }

    constexpr void scheduleBranch(std::uint32_t target) noexcept
    {
        branchTarget = target;
        delaySlotPending = true;
    }

    constexpr void finishInstruction(CpuState &cpu, bool wasDelaySlot) noexcept
    {
        cpu.advancePc();
        cpu.recordRetiredInstruction();
        if (wasDelaySlot && delaySlotPending)
        {
            cpu.pc = branchTarget;
            delaySlotPending = false;
        }
    }
};

// Branch target for the classic MIPS relative immediate: PC+4 + signext(imm16)*4.
[[nodiscard]] constexpr std::uint32_t relativeBranchTarget(
    std::uint32_t pc, std::uint16_t immediate) noexcept
{
    const auto signedOffset = static_cast<std::int32_t>(
        static_cast<std::int16_t>(immediate));
    return pc + 4u + static_cast<std::uint32_t>(signedOffset * 4);
}
} // namespace ps2::xbox360::ee
