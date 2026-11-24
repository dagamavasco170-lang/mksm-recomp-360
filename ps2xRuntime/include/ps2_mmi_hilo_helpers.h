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

// PMFHL.UW selects the upper word of each 64-bit HI/LO bank pair.
[[nodiscard]] constexpr Vector128 pmfhlUw(const HiLoState &state) noexcept
{
    return {{{state.lo.words[1], state.hi.words[1],
              state.lo.words[3], state.hi.words[3]}}};
}

[[nodiscard]] constexpr std::uint32_t pmfhlClampSignedWord(std::uint32_t value) noexcept
{
    const std::int32_t signedValue = std::bit_cast<std::int32_t>(value);
    if (signedValue > 0x7FFF)
        return 0x7FFFu;
    if (signedValue < -0x8000)
        return 0xFFFF8000u;
    return value;
}

[[nodiscard]] constexpr Vector128 pmfhlSh(const HiLoState &state) noexcept
{
    const std::array<std::uint32_t, 8> source{
        state.lo.words[0], state.lo.words[1], state.hi.words[0], state.hi.words[1],
        state.lo.words[2], state.lo.words[3], state.hi.words[2], state.hi.words[3]};
    std::array<std::uint16_t, 8> halfwords{};
    for (std::size_t i = 0; i < source.size(); ++i)
        halfwords[i] = static_cast<std::uint16_t>(pmfhlClampSignedWord(source[i]));
    return {{{static_cast<std::uint32_t>(halfwords[0]) |
                  (static_cast<std::uint32_t>(halfwords[1]) << 16u),
              static_cast<std::uint32_t>(halfwords[2]) |
                  (static_cast<std::uint32_t>(halfwords[3]) << 16u),
              static_cast<std::uint32_t>(halfwords[4]) |
                  (static_cast<std::uint32_t>(halfwords[5]) << 16u),
              static_cast<std::uint32_t>(halfwords[6]) |
                  (static_cast<std::uint32_t>(halfwords[7]) << 16u)}}};
}

// PMFHL.LH rearranges selected halfwords from HI/LO into the order specified
// by the EE manual: LO0, LO2, HI0, HI2, LO4, LO6, HI4, HI6.
[[nodiscard]] constexpr Vector128 pmfhlLh(const HiLoState &state) noexcept
{
    const std::array<std::uint16_t, 8> halfwords{
        static_cast<std::uint16_t>(state.lo.words[0]),
        static_cast<std::uint16_t>(state.lo.words[1] >> 16u),
        static_cast<std::uint16_t>(state.hi.words[0]),
        static_cast<std::uint16_t>(state.hi.words[1] >> 16u),
        static_cast<std::uint16_t>(state.lo.words[2]),
        static_cast<std::uint16_t>(state.lo.words[3] >> 16u),
        static_cast<std::uint16_t>(state.hi.words[2]),
        static_cast<std::uint16_t>(state.hi.words[3] >> 16u)};
    return {{{static_cast<std::uint32_t>(halfwords[0]) |
                  (static_cast<std::uint32_t>(halfwords[1]) << 16u),
              static_cast<std::uint32_t>(halfwords[2]) |
                  (static_cast<std::uint32_t>(halfwords[3]) << 16u),
              static_cast<std::uint32_t>(halfwords[4]) |
                  (static_cast<std::uint32_t>(halfwords[5]) << 16u),
              static_cast<std::uint32_t>(halfwords[6]) |
                  (static_cast<std::uint32_t>(halfwords[7]) << 16u)}}};
}

// PMFHL.SLW concatenates corresponding 32-bit HI/LO words into two signed
// 64-bit values, saturates each to signed 32-bit, and sign-extends the result.
[[nodiscard]] constexpr Vector128 pmfhlSlw(const HiLoState &state) noexcept
{
    const auto saturate = [](std::uint32_t hiWord, std::uint32_t loWord) constexpr {
        const std::uint64_t bits = (static_cast<std::uint64_t>(hiWord) << 32u) | loWord;
        const std::int64_t value = std::bit_cast<std::int64_t>(bits);
        if (value > 0x7FFFFFFFll)
            return 0x7FFFFFFFu;
        if (value < -0x80000000ll)
            return 0x80000000u;
        return static_cast<std::uint32_t>(value);
    };
    const std::uint32_t lane0 = saturate(state.hi.words[0], state.lo.words[0]);
    const std::uint32_t lane1 = saturate(state.hi.words[2], state.lo.words[2]);
    const std::uint32_t sign0 = (lane0 & 0x80000000u) ? 0xFFFFFFFFu : 0u;
    const std::uint32_t sign1 = (lane1 & 0x80000000u) ? 0xFFFFFFFFu : 0u;
    return {{{lane0, sign0, lane1, sign1}}};
}

// PMTHL.LW writes source words 0/2 into LO words 0/2 and source words 1/3
// into HI words 0/2. The intervening accumulator words are architecturally
// unchanged.
constexpr void pmthlLw(HiLoState &state, const Vector128 &source) noexcept
{
    state.lo.words[0] = source.words[0];
    state.hi.words[0] = source.words[1];
    state.lo.words[2] = source.words[2];
    state.hi.words[2] = source.words[3];
}
} // namespace ps2::mmi
