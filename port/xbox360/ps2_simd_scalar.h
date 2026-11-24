#pragma once

#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <type_traits>

namespace ps2::xbox360::simd
{
struct Vector128
{
    // PS2 guest byte order: bytes[0] is the lowest address and lane 0 is LSB.
    std::array<std::uint8_t, 16> bytes{};
};

template <typename Lane>
inline constexpr bool isSupportedLane =
    std::is_same_v<Lane, std::uint8_t> ||
    std::is_same_v<Lane, std::uint16_t> ||
    std::is_same_v<Lane, std::uint32_t>;

namespace detail
{
template <typename Lane> struct SignedLane;
template <> struct SignedLane<std::uint8_t> { using type = std::int8_t; };
template <> struct SignedLane<std::uint16_t> { using type = std::int16_t; };
template <> struct SignedLane<std::uint32_t> { using type = std::int32_t; };
template <typename Lane> using SignedLaneT = typename SignedLane<Lane>::type;

template <typename Lane>
[[nodiscard]] constexpr SignedLaneT<Lane> asSigned(Lane value) noexcept
{
    return std::bit_cast<SignedLaneT<Lane>>(value);
}
} // namespace detail

template <typename Lane>
[[nodiscard]] constexpr Lane readLane(const Vector128 &value, std::size_t lane) noexcept
{
    static_assert(isSupportedLane<Lane>);
    Lane result = 0;
    const std::size_t offset = lane * sizeof(Lane);
    for (std::size_t byte = 0; byte < sizeof(Lane); ++byte)
    {
        const Lane part = static_cast<Lane>(value.bytes[offset + byte]);
        result = static_cast<Lane>(result | static_cast<Lane>(part << (byte * 8u)));
    }
    return result;
}

template <typename Lane>
constexpr void writeLane(Vector128 &value, std::size_t lane, Lane laneValue) noexcept
{
    static_assert(isSupportedLane<Lane>);
    const std::size_t offset = lane * sizeof(Lane);
    for (std::size_t byte = 0; byte < sizeof(Lane); ++byte)
        value.bytes[offset + byte] =
            static_cast<std::uint8_t>((laneValue >> (byte * 8u)) & Lane{0xFFu});
}

template <typename Lane>
[[nodiscard]] constexpr Vector128 addLanes(const Vector128 &a, const Vector128 &b) noexcept
{
    static_assert(isSupportedLane<Lane>);
    Vector128 result{};
    constexpr std::size_t count = 16u / sizeof(Lane);
    for (std::size_t lane = 0; lane < count; ++lane)
        writeLane(result, lane, static_cast<Lane>(readLane<Lane>(a, lane) +
                                                  readLane<Lane>(b, lane)));
    return result;
}

template <typename Lane>
[[nodiscard]] constexpr Vector128 subtractLanes(const Vector128 &a, const Vector128 &b) noexcept
{
    static_assert(isSupportedLane<Lane>);
    Vector128 result{};
    constexpr std::size_t count = 16u / sizeof(Lane);
    for (std::size_t lane = 0; lane < count; ++lane)
        writeLane(result, lane, static_cast<Lane>(readLane<Lane>(a, lane) -
                                                  readLane<Lane>(b, lane)));
    return result;
}

[[nodiscard]] constexpr Vector128 bitAnd(const Vector128 &a, const Vector128 &b) noexcept
{
    Vector128 r{};
    for (std::size_t i = 0; i < 16u; ++i) r.bytes[i] = a.bytes[i] & b.bytes[i];
    return r;
}
[[nodiscard]] constexpr Vector128 bitOr(const Vector128 &a, const Vector128 &b) noexcept
{
    Vector128 r{};
    for (std::size_t i = 0; i < 16u; ++i) r.bytes[i] = a.bytes[i] | b.bytes[i];
    return r;
}
[[nodiscard]] constexpr Vector128 bitXor(const Vector128 &a, const Vector128 &b) noexcept
{
    Vector128 r{};
    for (std::size_t i = 0; i < 16u; ++i) r.bytes[i] = a.bytes[i] ^ b.bytes[i];
    return r;
}
[[nodiscard]] constexpr Vector128 bitNor(const Vector128 &a, const Vector128 &b) noexcept
{
    Vector128 r{};
    for (std::size_t i = 0; i < 16u; ++i)
        r.bytes[i] = static_cast<std::uint8_t>(~(a.bytes[i] | b.bytes[i]));
    return r;
}

template <typename Lane>
[[nodiscard]] constexpr Vector128 compareEqualLanes(const Vector128 &a,
                                                     const Vector128 &b) noexcept
{
    static_assert(isSupportedLane<Lane>);
    Vector128 r{};
    constexpr std::size_t count = 16u / sizeof(Lane);
    for (std::size_t i = 0; i < count; ++i)
        writeLane(r, i, readLane<Lane>(a, i) == readLane<Lane>(b, i)
                            ? std::numeric_limits<Lane>::max() : Lane{0});
    return r;
}

template <typename Lane>
[[nodiscard]] constexpr Vector128 compareGreaterSigned(const Vector128 &a,
                                                         const Vector128 &b) noexcept
{
    static_assert(isSupportedLane<Lane>);
    Vector128 r{};
    constexpr std::size_t count = 16u / sizeof(Lane);
    for (std::size_t i = 0; i < count; ++i)
    {
        const Lane lhs = readLane<Lane>(a, i), rhs = readLane<Lane>(b, i);
        writeLane(r, i, detail::asSigned(lhs) > detail::asSigned(rhs)
                            ? std::numeric_limits<Lane>::max() : Lane{0});
    }
    return r;
}

template <typename Lane>
[[nodiscard]] constexpr Vector128 minSigned(const Vector128 &a, const Vector128 &b) noexcept
{
    static_assert(isSupportedLane<Lane>);
    Vector128 r{};
    constexpr std::size_t count = 16u / sizeof(Lane);
    for (std::size_t i = 0; i < count; ++i)
    {
        const Lane lhs = readLane<Lane>(a, i), rhs = readLane<Lane>(b, i);
        writeLane(r, i, detail::asSigned(lhs) < detail::asSigned(rhs) ? lhs : rhs);
    }
    return r;
}

template <typename Lane>
[[nodiscard]] constexpr Vector128 maxSigned(const Vector128 &a, const Vector128 &b) noexcept
{
    static_assert(isSupportedLane<Lane>);
    Vector128 r{};
    constexpr std::size_t count = 16u / sizeof(Lane);
    for (std::size_t i = 0; i < count; ++i)
    {
        const Lane lhs = readLane<Lane>(a, i), rhs = readLane<Lane>(b, i);
        writeLane(r, i, detail::asSigned(lhs) > detail::asSigned(rhs) ? lhs : rhs);
    }
    return r;
}

template <typename Lane>
[[nodiscard]] constexpr Vector128 absoluteSigned(const Vector128 &v) noexcept
{
    static_assert(isSupportedLane<Lane>);
    Vector128 r{};
    constexpr std::size_t count = 16u / sizeof(Lane);
    for (std::size_t i = 0; i < count; ++i)
    {
        const Lane raw = readLane<Lane>(v, i);
        const Lane absolute = detail::asSigned(raw) < 0
                                  ? static_cast<Lane>(Lane{0} - raw)
                                  : raw;
        writeLane<Lane>(r, i, absolute);
    }
    return r;
}

template <typename Lane>
[[nodiscard]] constexpr Vector128 addSaturatingSignedLanes(const Vector128 &a,
                                                           const Vector128 &b) noexcept
{
    static_assert(isSupportedLane<Lane>);
    using Signed = detail::SignedLaneT<Lane>;
    Vector128 r{};
    constexpr std::size_t count = 16u / sizeof(Lane);
    constexpr std::int64_t minimum = std::numeric_limits<Signed>::min();
    constexpr std::int64_t maximum = std::numeric_limits<Signed>::max();
    for (std::size_t i = 0; i < count; ++i)
    {
        const std::int64_t lhs = detail::asSigned(readLane<Lane>(a, i));
        const std::int64_t rhs = detail::asSigned(readLane<Lane>(b, i));
        const std::int64_t value = lhs + rhs;
        const auto clamped = value > maximum ? maximum : (value < minimum ? minimum : value);
        writeLane<Lane>(r, i, std::bit_cast<Lane>(static_cast<Signed>(clamped)));
    }
    return r;
}

template <typename Lane>
[[nodiscard]] constexpr Vector128 subtractSaturatingSignedLanes(const Vector128 &a,
                                                                const Vector128 &b) noexcept
{
    static_assert(isSupportedLane<Lane>);
    using Signed = detail::SignedLaneT<Lane>;
    Vector128 r{};
    constexpr std::size_t count = 16u / sizeof(Lane);
    constexpr std::int64_t minimum = std::numeric_limits<Signed>::min();
    constexpr std::int64_t maximum = std::numeric_limits<Signed>::max();
    for (std::size_t i = 0; i < count; ++i)
    {
        const std::int64_t lhs = detail::asSigned(readLane<Lane>(a, i));
        const std::int64_t rhs = detail::asSigned(readLane<Lane>(b, i));
        const std::int64_t value = lhs - rhs;
        const auto clamped = value > maximum ? maximum : (value < minimum ? minimum : value);
        writeLane<Lane>(r, i, std::bit_cast<Lane>(static_cast<Signed>(clamped)));
    }
    return r;
}

template <typename Lane>
[[nodiscard]] constexpr Vector128 addSaturatingUnsignedLanes(const Vector128 &a,
                                                             const Vector128 &b) noexcept
{
    static_assert(isSupportedLane<Lane>);
    Vector128 r{};
    constexpr std::size_t count = 16u / sizeof(Lane);
    constexpr std::uint64_t maximum = std::numeric_limits<Lane>::max();
    for (std::size_t i = 0; i < count; ++i)
    {
        const std::uint64_t value =
            static_cast<std::uint64_t>(readLane<Lane>(a, i)) +
            static_cast<std::uint64_t>(readLane<Lane>(b, i));
        writeLane<Lane>(r, i, static_cast<Lane>(value > maximum ? maximum : value));
    }
    return r;
}

template <typename Lane>
[[nodiscard]] constexpr Vector128 subtractSaturatingUnsignedLanes(const Vector128 &a,
                                                                  const Vector128 &b) noexcept
{
    static_assert(isSupportedLane<Lane>);
    Vector128 r{};
    constexpr std::size_t count = 16u / sizeof(Lane);
    for (std::size_t i = 0; i < count; ++i)
    {
        const Lane lhs = readLane<Lane>(a, i);
        const Lane rhs = readLane<Lane>(b, i);
        writeLane<Lane>(r, i, lhs < rhs ? Lane{0} : static_cast<Lane>(lhs - rhs));
    }
    return r;
}

[[nodiscard]] constexpr Vector128 addSaturatingSigned32(const Vector128 &a,
                                                         const Vector128 &b) noexcept
{
    return addSaturatingSignedLanes<std::uint32_t>(a, b);
}

[[nodiscard]] constexpr Vector128 subtractSaturatingSigned32(const Vector128 &a,
                                                              const Vector128 &b) noexcept
{
    return subtractSaturatingSignedLanes<std::uint32_t>(a, b);
}

struct PmaddhResult
{
    Vector128 destination{};
    Vector128 hi{};
    Vector128 lo{};
};

// PS2 PMADDH: eight independent signed-halfword products are accumulated into
// eight corresponding 32-bit words spread across the 128-bit HI and LO banks.
// The GPR destination receives LO[0], HI[0], LO[2], HI[2], in that order.
[[nodiscard]] constexpr PmaddhResult pmaddh(const Vector128 &rs,
                                             const Vector128 &rt,
                                             const Vector128 &initialHi,
                                             const Vector128 &initialLo) noexcept
{
    PmaddhResult result{};
    result.hi = initialHi;
    result.lo = initialLo;

    for (std::size_t product = 0; product < 8u; ++product)
    {
        const std::int64_t lhs = detail::asSigned(readLane<std::uint16_t>(rs, product));
        const std::int64_t rhs = detail::asSigned(readLane<std::uint16_t>(rt, product));
        const std::uint32_t productBits = static_cast<std::uint32_t>(lhs * rhs);

        if (product < 2u)
        {
            const std::uint32_t accum = readLane<std::uint32_t>(result.lo, product);
            writeLane<std::uint32_t>(result.lo, product, accum + productBits);
        }
        else if (product < 4u)
        {
            const std::size_t lane = product - 2u;
            const std::uint32_t accum = readLane<std::uint32_t>(result.hi, lane);
            writeLane<std::uint32_t>(result.hi, lane, accum + productBits);
        }
        else if (product < 6u)
        {
            const std::size_t lane = product - 2u;
            const std::uint32_t accum = readLane<std::uint32_t>(result.lo, lane);
            writeLane<std::uint32_t>(result.lo, lane, accum + productBits);
        }
        else
        {
            const std::size_t lane = product - 4u;
            const std::uint32_t accum = readLane<std::uint32_t>(result.hi, lane);
            writeLane<std::uint32_t>(result.hi, lane, accum + productBits);
        }
    }

    writeLane<std::uint32_t>(result.destination, 0u, readLane<std::uint32_t>(result.lo, 0u));
    writeLane<std::uint32_t>(result.destination, 1u, readLane<std::uint32_t>(result.hi, 0u));
    writeLane<std::uint32_t>(result.destination, 2u, readLane<std::uint32_t>(result.lo, 2u));
    writeLane<std::uint32_t>(result.destination, 3u, readLane<std::uint32_t>(result.hi, 2u));
    return result;
}

// PS2 PMSUBH subtracts the same signed-halfword products from the HI/LO
// accumulator words used by PMADDH. Unsigned lane arithmetic defines wraparound.
[[nodiscard]] constexpr PmaddhResult pmsubh(const Vector128 &rs,
                                             const Vector128 &rt,
                                             const Vector128 &initialHi,
                                             const Vector128 &initialLo) noexcept
{
    PmaddhResult result{};
    result.hi = initialHi;
    result.lo = initialLo;

    for (std::size_t product = 0; product < 8u; ++product)
    {
        const std::int64_t lhs = detail::asSigned(readLane<std::uint16_t>(rs, product));
        const std::int64_t rhs = detail::asSigned(readLane<std::uint16_t>(rt, product));
        const std::uint32_t productBits = static_cast<std::uint32_t>(lhs * rhs);

        if (product < 2u)
        {
            const std::uint32_t accum = readLane<std::uint32_t>(result.lo, product);
            writeLane<std::uint32_t>(result.lo, product, accum - productBits);
        }
        else if (product < 4u)
        {
            const std::size_t lane = product - 2u;
            const std::uint32_t accum = readLane<std::uint32_t>(result.hi, lane);
            writeLane<std::uint32_t>(result.hi, lane, accum - productBits);
        }
        else if (product < 6u)
        {
            const std::size_t lane = product - 2u;
            const std::uint32_t accum = readLane<std::uint32_t>(result.lo, lane);
            writeLane<std::uint32_t>(result.lo, lane, accum - productBits);
        }
        else
        {
            const std::size_t lane = product - 4u;
            const std::uint32_t accum = readLane<std::uint32_t>(result.hi, lane);
            writeLane<std::uint32_t>(result.hi, lane, accum - productBits);
        }
    }

    writeLane<std::uint32_t>(result.destination, 0u, readLane<std::uint32_t>(result.lo, 0u));
    writeLane<std::uint32_t>(result.destination, 1u, readLane<std::uint32_t>(result.hi, 0u));
    writeLane<std::uint32_t>(result.destination, 2u, readLane<std::uint32_t>(result.lo, 2u));
    writeLane<std::uint32_t>(result.destination, 3u, readLane<std::uint32_t>(result.hi, 2u));
    return result;
}

[[nodiscard]] constexpr Vector128 shiftLeftVariable32(const Vector128 &v,
                                                       const Vector128 &counts) noexcept
{
    Vector128 r{};
    for (std::size_t i = 0; i < 4u; ++i)
    {
        const auto n = readLane<std::uint32_t>(counts, i) & 0x1Fu;
        writeLane(r, i, readLane<std::uint32_t>(v, i) << n);
    }
    return r;
}

[[nodiscard]] constexpr Vector128 shiftRightLogicalVariable32(const Vector128 &v,
                                                               const Vector128 &counts) noexcept
{
    Vector128 r{};
    for (std::size_t i = 0; i < 4u; ++i)
    {
        const auto n = readLane<std::uint32_t>(counts, i) & 0x1Fu;
        writeLane(r, i, readLane<std::uint32_t>(v, i) >> n);
    }
    return r;
}

[[nodiscard]] constexpr Vector128 shiftRightArithmeticVariable32(const Vector128 &v,
                                                                  const Vector128 &counts) noexcept
{
    Vector128 r{};
    for (std::size_t i = 0; i < 4u; ++i)
    {
        const auto n = readLane<std::uint32_t>(counts, i) & 0x1Fu;
        const std::int32_t signedValue = detail::asSigned(readLane<std::uint32_t>(v, i));
        writeLane(r, i, std::bit_cast<std::uint32_t>(static_cast<std::int32_t>(signedValue >> n)));
    }
    return r;
}

[[nodiscard]] constexpr Vector128 packSigned32To16(const Vector128 &a,
                                                    const Vector128 &b) noexcept
{
    Vector128 r{};
    for (std::size_t i = 0; i < 4u; ++i)
    {
        const auto lhs = detail::asSigned(readLane<std::uint32_t>(a, i));
        const auto rhs = detail::asSigned(readLane<std::uint32_t>(b, i));
        const auto clamp = [](std::int32_t n) constexpr {
            return n > std::numeric_limits<std::int16_t>::max()
                       ? std::numeric_limits<std::int16_t>::max()
                       : (n < std::numeric_limits<std::int16_t>::min()
                              ? std::numeric_limits<std::int16_t>::min() : n);
        };
        writeLane(r, i, std::bit_cast<std::uint16_t>(static_cast<std::int16_t>(clamp(lhs))));
        writeLane(r, i + 4u, std::bit_cast<std::uint16_t>(static_cast<std::int16_t>(clamp(rhs))));
    }
    return r;
}

template <typename Lane>
[[nodiscard]] constexpr Vector128 packEvenLanes(const Vector128 &a,
                                                 const Vector128 &b) noexcept
{
    static_assert(isSupportedLane<Lane>);
    Vector128 r{};
    constexpr std::size_t half = (16u / sizeof(Lane)) / 2u;
    for (std::size_t i = 0; i < half; ++i)
    {
        writeLane(r, i, readLane<Lane>(b, i * 2u));
        writeLane(r, i + half, readLane<Lane>(a, i * 2u));
    }
    return r;
}

template <typename Lane>
[[nodiscard]] constexpr Vector128 interleaveLow(const Vector128 &a,
                                                 const Vector128 &b) noexcept
{
    static_assert(isSupportedLane<Lane>);
    Vector128 r{};
    constexpr std::size_t half = 8u / sizeof(Lane);
    for (std::size_t i = 0; i < half; ++i)
    {
        writeLane(r, i * 2u, readLane<Lane>(b, i));
        writeLane(r, i * 2u + 1u, readLane<Lane>(a, i));
    }
    return r;
}

template <typename Lane>
[[nodiscard]] constexpr Vector128 interleaveHigh(const Vector128 &a,
                                                  const Vector128 &b) noexcept
{
    static_assert(isSupportedLane<Lane>);
    Vector128 r{};
    constexpr std::size_t half = 8u / sizeof(Lane);
    for (std::size_t i = 0; i < half; ++i)
    {
        writeLane(r, i * 2u, readLane<Lane>(b, i + half));
        writeLane(r, i * 2u + 1u, readLane<Lane>(a, i + half));
    }
    return r;
}
} // namespace ps2::xbox360::simd
