#pragma once

#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>

namespace ps2::mmi
{
// Byte-order-independent representation of the PS2 EE's four 32-bit words.
struct Vector128
{
    std::array<std::uint32_t, 4> words{};
};

struct HiLoState
{
    Vector128 hi{};
    Vector128 lo{};
};

static_assert(sizeof(Vector128) == 16u);

[[nodiscard]] constexpr Vector128 from64Halves(std::uint64_t low,
                                                std::uint64_t high) noexcept
{
    return {{{static_cast<std::uint32_t>(low),
              static_cast<std::uint32_t>(low >> 32u),
              static_cast<std::uint32_t>(high),
              static_cast<std::uint32_t>(high >> 32u)}}};
}

constexpr void to64Halves(const Vector128 &value,
                          std::uint64_t &low,
                          std::uint64_t &high) noexcept
{
    low = static_cast<std::uint64_t>(value.words[0]) |
          (static_cast<std::uint64_t>(value.words[1]) << 32u);
    high = static_cast<std::uint64_t>(value.words[2]) |
           (static_cast<std::uint64_t>(value.words[3]) << 32u);
}

[[nodiscard]] constexpr Vector128 pmfhi(const HiLoState &state) noexcept
{
    return state.hi;
}

[[nodiscard]] constexpr Vector128 pmflo(const HiLoState &state) noexcept
{
    return state.lo;
}

constexpr void pmthi(HiLoState &state, const Vector128 &source) noexcept
{
    state.hi = source;
}

constexpr void pmtlo(HiLoState &state, const Vector128 &source) noexcept
{
    state.lo = source;
}

// PMADDH adds eight signed-halfword products to eight independent 32-bit
// accumulator lanes. Product pairs 0/1 update LO words 0/1; 2/3 update HI
// words 0/1; 4/5 update LO words 2/3; and 6/7 update HI words 2/3.
// Unsigned word addition intentionally gives the EE's modulo-2^32 wraparound.
[[nodiscard]] constexpr Vector128 pmaddh(HiLoState &state,
                                         const Vector128 &rs,
                                         const Vector128 &rt) noexcept
{
    for (std::size_t product = 0; product < 8u; ++product)
    {
        const std::size_t word = product / 2u;
        const unsigned shift = (product & 1u) * 16u;
        const auto lhsBits = static_cast<std::uint16_t>(rs.words[word] >> shift);
        const auto rhsBits = static_cast<std::uint16_t>(rt.words[word] >> shift);
        const std::int32_t lhs = std::bit_cast<std::int16_t>(lhsBits);
        const std::int32_t rhs = std::bit_cast<std::int16_t>(rhsBits);
        const std::uint32_t productBits = static_cast<std::uint32_t>(lhs * rhs);

        if (product < 2u)
            state.lo.words[product] += productBits;
        else if (product < 4u)
            state.hi.words[product - 2u] += productBits;
        else if (product < 6u)
            state.lo.words[product - 2u] += productBits;
        else
            state.hi.words[product - 4u] += productBits;
    }

    return {{{state.lo.words[0], state.hi.words[0],
              state.lo.words[2], state.hi.words[2]}}};
}

// PMSUBH subtracts each signed-halfword product from the same eight independent
// 32-bit accumulator words used by PMADDH. Unsigned subtraction intentionally
// gives modulo-2^32 wraparound without signed-overflow undefined behavior.
[[nodiscard]] constexpr Vector128 pmsubh(HiLoState &state,
                                         const Vector128 &rs,
                                         const Vector128 &rt) noexcept
{
    for (std::size_t product = 0; product < 8u; ++product)
    {
        const std::size_t word = product / 2u;
        const unsigned shift = (product & 1u) * 16u;
        const auto lhsBits = static_cast<std::uint16_t>(rs.words[word] >> shift);
        const auto rhsBits = static_cast<std::uint16_t>(rt.words[word] >> shift);
        const std::int32_t lhs = std::bit_cast<std::int16_t>(lhsBits);
        const std::int32_t rhs = std::bit_cast<std::int16_t>(rhsBits);
        const std::uint32_t productBits = static_cast<std::uint32_t>(lhs * rhs);

        if (product < 2u)
            state.lo.words[product] -= productBits;
        else if (product < 4u)
            state.hi.words[product - 2u] -= productBits;
        else if (product < 6u)
            state.lo.words[product - 2u] -= productBits;
        else
            state.hi.words[product - 4u] -= productBits;
    }

    return {{{state.lo.words[0], state.hi.words[0],
              state.lo.words[2], state.hi.words[2]}}};
}

// PMFHL.LW copies the low/high HI/LO word pair from both halves of each
// 128-bit bank to a 128-bit GPR: LO0, HI0, LO2, HI2.
[[nodiscard]] constexpr Vector128 pmfhlLw(const HiLoState &state) noexcept
{
    return {{{state.lo.words[0], state.hi.words[0],
              state.lo.words[2], state.hi.words[2]}}};
}
} // namespace ps2::mmi
