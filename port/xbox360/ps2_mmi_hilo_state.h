#pragma once

#include "ps2_simd_scalar.h"

#include <cstddef>
#include <cstdint>

namespace ps2::xbox360::mmi
{
// Portable storage for the PS2 MMI 128-bit HI/LO accumulator banks.
// This intentionally models storage only; instruction-specific transfer and
// arithmetic semantics must be implemented and tested separately.
struct HiLoState
{
    simd::Vector128 hi{};
    simd::Vector128 lo{};

    [[nodiscard]] constexpr std::uint32_t hiWord(std::size_t lane) const noexcept
    {
        return simd::readLane<std::uint32_t>(hi, lane);
    }

    [[nodiscard]] constexpr std::uint32_t loWord(std::size_t lane) const noexcept
    {
        return simd::readLane<std::uint32_t>(lo, lane);
    }

    constexpr void setHiWord(std::size_t lane, std::uint32_t value) noexcept
    {
        simd::writeLane<std::uint32_t>(hi, lane, value);
    }

    constexpr void setLoWord(std::size_t lane, std::uint32_t value) noexcept
    {
        simd::writeLane<std::uint32_t>(lo, lane, value);
    }
};
} // namespace ps2::xbox360::mmi
