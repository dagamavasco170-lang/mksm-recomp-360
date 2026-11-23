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
struct CpuState
{
    std::array<simd::Vector128, 32> gpr{};
    mmi::HiLoState hilo{};
    std::uint32_t pc{};
    std::uint64_t retiredInstructions{};

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

    constexpr void recordRetiredInstruction() noexcept
    {
        ++retiredInstructions;
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
