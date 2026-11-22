#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <type_traits>

namespace ps2::xbox360::simd
{
// Scalar reference for a 128-bit PS2 guest vector.
// bytes[0] is the lowest guest address; lane 0 is the least-significant lane.
// This deliberately avoids native host vector types and host endianness.
struct Vector128
{
    std::array<std::uint8_t, 16> bytes{};
};

template <typename Lane>
inline constexpr bool isSupportedLane =
    std::is_same_v<Lane, std::uint8_t> ||
    std::is_same_v<Lane, std::uint16_t> ||
    std::is_same_v<Lane, std::uint32_t>;

template <typename Lane>
[[nodiscard]] constexpr Lane readLane(const Vector128 &value, std::size_t lane) noexcept
{
    static_assert(isSupportedLane<Lane>,
                  "PS2 packed integer lanes support unsigned 8/16/32-bit values");
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
    static_assert(isSupportedLane<Lane>,
                  "PS2 packed integer lanes support unsigned 8/16/32-bit values");
    const std::size_t offset = lane * sizeof(Lane);
    for (std::size_t byte = 0; byte < sizeof(Lane); ++byte)
    {
        value.bytes[offset + byte] =
            static_cast<std::uint8_t>((laneValue >> (byte * 8u)) & Lane{0xFFu});
    }
}

// Wrapping, lane-wise arithmetic matching the non-saturating SSE operations
// currently used by PS2_PADDB/PADDH/PADDW and PS2_PSUBB/PSUBH/PSUBW.
template <typename Lane>
[[nodiscard]] constexpr Vector128 addLanes(const Vector128 &a,
                                            const Vector128 &b) noexcept
{
    static_assert(isSupportedLane<Lane>);
    Vector128 result{};
    constexpr std::size_t count = 16u / sizeof(Lane);
    for (std::size_t lane = 0; lane < count; ++lane)
    {
        const Lane sum = static_cast<Lane>(readLane<Lane>(a, lane) +
                                           readLane<Lane>(b, lane));
        writeLane(result, lane, sum);
    }
    return result;
}

template <typename Lane>
[[nodiscard]] constexpr Vector128 subtractLanes(const Vector128 &a,
                                                 const Vector128 &b) noexcept
{
    static_assert(isSupportedLane<Lane>);
    Vector128 result{};
    constexpr std::size_t count = 16u / sizeof(Lane);
    for (std::size_t lane = 0; lane < count; ++lane)
    {
        const Lane difference = static_cast<Lane>(readLane<Lane>(a, lane) -
                                                  readLane<Lane>(b, lane));
        writeLane(result, lane, difference);
    }
    return result;
}

[[nodiscard]] constexpr Vector128 bitAnd(const Vector128 &a, const Vector128 &b) noexcept
{
    Vector128 result{};
    for (std::size_t i = 0; i < result.bytes.size(); ++i)
        result.bytes[i] = static_cast<std::uint8_t>(a.bytes[i] & b.bytes[i]);
    return result;
}

[[nodiscard]] constexpr Vector128 bitOr(const Vector128 &a, const Vector128 &b) noexcept
{
    Vector128 result{};
    for (std::size_t i = 0; i < result.bytes.size(); ++i)
        result.bytes[i] = static_cast<std::uint8_t>(a.bytes[i] | b.bytes[i]);
    return result;
}

[[nodiscard]] constexpr Vector128 bitXor(const Vector128 &a, const Vector128 &b) noexcept
{
    Vector128 result{};
    for (std::size_t i = 0; i < result.bytes.size(); ++i)
        result.bytes[i] = static_cast<std::uint8_t>(a.bytes[i] ^ b.bytes[i]);
    return result;
}

[[nodiscard]] constexpr Vector128 bitNor(const Vector128 &a, const Vector128 &b) noexcept
{
    Vector128 result{};
    for (std::size_t i = 0; i < result.bytes.size(); ++i)
        result.bytes[i] = static_cast<std::uint8_t>(~(a.bytes[i] | b.bytes[i]));
    return result;
}

// Equal lanes become an all-one bit mask; unequal lanes become zero.
template <typename Lane>
[[nodiscard]] constexpr Vector128 compareEqualLanes(const Vector128 &a,
                                                     const Vector128 &b) noexcept
{
    static_assert(isSupportedLane<Lane>);
    Vector128 result{};
    constexpr std::size_t count = 16u / sizeof(Lane);
    for (std::size_t lane = 0; lane < count; ++lane)
    {
        const Lane mask = readLane<Lane>(a, lane) == readLane<Lane>(b, lane)
                              ? std::numeric_limits<Lane>::max()
                              : Lane{0};
        writeLane(result, lane, mask);
    }
    return result;
}

// Exact argument ordering of the existing macros:
// PS2_PEXT*(a,b) maps to unpack(b,a), so each output pair is [b[i], a[i]].
template <typename Lane>
[[nodiscard]] constexpr Vector128 interleaveLow(const Vector128 &a,
                                                 const Vector128 &b) noexcept
{
    static_assert(isSupportedLane<Lane>);
    Vector128 result{};
    constexpr std::size_t half = 8u / sizeof(Lane);
    for (std::size_t lane = 0; lane < half; ++lane)
    {
        writeLane(result, lane * 2u, readLane<Lane>(b, lane));
        writeLane(result, lane * 2u + 1u, readLane<Lane>(a, lane));
    }
    return result;
}

template <typename Lane>
[[nodiscard]] constexpr Vector128 interleaveHigh(const Vector128 &a,
                                                  const Vector128 &b) noexcept
{
    static_assert(isSupportedLane<Lane>);
    Vector128 result{};
    constexpr std::size_t half = 8u / sizeof(Lane);
    for (std::size_t lane = 0; lane < half; ++lane)
    {
        writeLane(result, lane * 2u, readLane<Lane>(b, lane + half));
        writeLane(result, lane * 2u + 1u, readLane<Lane>(a, lane + half));
    }
    return result;
}
} // namespace ps2::xbox360::simd
