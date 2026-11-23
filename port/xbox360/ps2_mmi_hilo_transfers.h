#pragma once

#include "ps2_mmi_hilo_state.h"

namespace ps2::xbox360::mmi
{
// PS2 EE MMI PMFHI/PMFLO and PMTHI/PMTLO move complete 128-bit banks.
// Semantics are documented in the EE Core Instruction Set Manual (PMFHI/PMFLO,
// PMTHI/PMTLO). The byte-defined Vector128 avoids host-endian assumptions.
// These helpers model transfers only; decoder/context integration is separate.
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


// Names matching the PS2 instruction manual, kept as explicit aliases.
[[nodiscard]] constexpr simd::Vector128 pmfhi(const HiLoState &state) noexcept { return mfhi(state); }
[[nodiscard]] constexpr simd::Vector128 pmflo(const HiLoState &state) noexcept { return mflo(state); }
constexpr void pmthi(HiLoState &state, const simd::Vector128 &source) noexcept { mthi(state, source); }
constexpr void pmtlo(HiLoState &state, const simd::Vector128 &source) noexcept { mtlo(state, source); }
} // namespace ps2::xbox360::mmi
