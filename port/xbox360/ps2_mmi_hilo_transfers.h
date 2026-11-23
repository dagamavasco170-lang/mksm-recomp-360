#pragma once

#include "ps2_mmi_hilo_state.h"

namespace ps2::xbox360::mmi
{
// MMI HI/LO transfer operations move the complete 128-bit register bank.
// The guest GPR is represented as the same byte-defined Vector128, avoiding
// host-endian or x86 register assumptions.
[[nodiscard]] constexpr simd::Vector128 mfhi(const HiLoState &state) noexcept
{
    return state.hi;
}

[[nodiscard]] constexpr simd::Vector128 mflo(const HiLoState &state) noexcept
{
    return state.lo;
}

constexpr void mthi(HiLoState &state, const simd::Vector128 &source) noexcept
{
    state.hi = source;
}

constexpr void mtlo(HiLoState &state, const simd::Vector128 &source) noexcept
{
    state.lo = source;
}
} // namespace ps2::xbox360::mmi
