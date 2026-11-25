#pragma once

#include "ps2_mmi_hilo_state.h"
#include "ps2_mmi_hilo_transfers.h"

#include <array>
#include <cstddef>
#include <cstdint>

namespace ps2::xbox360::ee
{
// Portable, byte-defined execution state for the Emotion Engine.
// This is a data model only; instruction execution and pipeline timing are separate.
struct Cop0State
{
    std::uint32_t status{};
    std::uint32_t cause{};
    std::uint32_t epc{};
    std::uint32_t badVaddr{};
};

struct CpuState
{
    static constexpr std::uint32_t StatusExl = 1u << 1u;
    static constexpr std::uint32_t StatusBev = 1u << 22u;
    static constexpr std::uint32_t CauseBd = 1u << 31u;
    static constexpr std::uint32_t CauseExcCodeMask = 0x7Cu;

    std::array<simd::Vector128, 32> gpr{};
    mmi::HiLoState hilo{};
    Cop0State cop0{};
    std::uint32_t pc{};
    std::uint64_t retiredInstructions{};
    bool branchDelayPending{};
    std::uint32_t branchTarget{};

    [[nodiscard]] constexpr const simd::Vector128 &readGpr(std::size_t index) const noexcept
    {
        return gpr[index & 31u];
    }

    constexpr void writeGpr(std::size_t index, const simd::Vector128 &value) noexcept
    {
        index &= 31u;
        if (index != 0u)
            gpr[index] = value;
    }

    constexpr void advancePc(std::uint32_t amount = 4u) noexcept
    {
        pc += amount;
    }

    constexpr void scheduleBranch(std::uint32_t target) noexcept
    {
        branchTarget = target;
        branchDelayPending = true;
    }

    constexpr void finishDelaySlot(bool wasDelaySlot) noexcept
    {
        if (wasDelaySlot && branchDelayPending)
        {
            pc = branchTarget;
            branchDelayPending = false;
        }
    }

    constexpr void recordRetiredInstruction() noexcept
    {
        ++retiredInstructions;
    }

    // Enter an EE level-1 exception. In a delay slot, EPC identifies the branch.
    constexpr void raiseException(std::uint8_t exceptionCode, bool wasDelaySlot) noexcept
    {
        const bool alreadyExl = (cop0.status & StatusExl) != 0u;
        cop0.cause &= ~(CauseExcCodeMask | CauseBd);
        cop0.cause |= (static_cast<std::uint32_t>(exceptionCode) << 2u) & CauseExcCodeMask;
        if (wasDelaySlot)
            cop0.cause |= CauseBd;

        if (!alreadyExl)
        {
            cop0.epc = wasDelaySlot ? pc - 4u : pc;
            const std::uint32_t modeStack = cop0.status & 0x3Fu;
            cop0.status = (cop0.status & ~0x3Fu) | ((modeStack << 2u) & 0x3Fu) | StatusExl;
        }
        pc = (cop0.status & StatusBev) != 0u ? 0xBFC00380u : 0x80000180u;
        branchDelayPending = false;
        branchTarget = 0u;
    }

    constexpr void pmfhi(std::size_t rd) noexcept
    {
        writeGpr(rd, mmi::pmfhi(hilo));
    }

    constexpr void pmflo(std::size_t rd) noexcept
    {
        writeGpr(rd, mmi::pmflo(hilo));
    }

    constexpr void pmthi(std::size_t rs) noexcept
    {
        mmi::pmthi(hilo, readGpr(rs));
    }

    constexpr void pmtlo(std::size_t rs) noexcept
    {
        mmi::pmtlo(hilo, readGpr(rs));
    }
};
} // namespace ps2::xbox360::ee
