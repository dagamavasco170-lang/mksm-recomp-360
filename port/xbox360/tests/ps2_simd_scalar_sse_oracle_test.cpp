#include "ps2_simd_scalar.h"

#include <emmintrin.h>

#include <array>
#include <cassert>
#include <cstdint>
#include <cstring>
#include <type_traits>

using ps2::xbox360::simd::Vector128;
using namespace ps2::xbox360::simd;

static_assert(sizeof(Vector128) == sizeof(__m128i));

namespace
{
__m128i toNative(const Vector128 &value) noexcept
{
    __m128i result{};
    std::memcpy(&result, value.bytes.data(), sizeof(result));
    return result;
}

void expectEqual(const Vector128 &actual, __m128i expected) noexcept
{
    Vector128 expectedBytes{};
    std::memcpy(expectedBytes.bytes.data(), &expected, sizeof(expected));
    assert(actual.bytes == expectedBytes.bytes);
}

template <typename Lane>
__m128i refAdd(__m128i a, __m128i b) noexcept
{
    if constexpr (std::is_same_v<Lane, std::uint8_t>)
        return _mm_add_epi8(a, b);
    else if constexpr (std::is_same_v<Lane, std::uint16_t>)
        return _mm_add_epi16(a, b);
    else
        return _mm_add_epi32(a, b);
}

template <typename Lane>
__m128i refSubtract(__m128i a, __m128i b) noexcept
{
    if constexpr (std::is_same_v<Lane, std::uint8_t>)
        return _mm_sub_epi8(a, b);
    else if constexpr (std::is_same_v<Lane, std::uint16_t>)
        return _mm_sub_epi16(a, b);
    else
        return _mm_sub_epi32(a, b);
}

template <typename Lane>
__m128i refEqual(__m128i a, __m128i b) noexcept
{
    if constexpr (std::is_same_v<Lane, std::uint8_t>)
        return _mm_cmpeq_epi8(a, b);
    else if constexpr (std::is_same_v<Lane, std::uint16_t>)
        return _mm_cmpeq_epi16(a, b);
    else
        return _mm_cmpeq_epi32(a, b);
}

template <typename Lane>
__m128i refGreaterSigned(__m128i a, __m128i b) noexcept
{
    if constexpr (std::is_same_v<Lane, std::uint8_t>)
        return _mm_cmpgt_epi8(a, b);
    else if constexpr (std::is_same_v<Lane, std::uint16_t>)
        return _mm_cmpgt_epi16(a, b);
    else
        return _mm_cmpgt_epi32(a, b);
}

template <typename Lane>
__m128i refUnpackLowReversed(__m128i a, __m128i b) noexcept
{
    // The scalar helper deliberately places the second input in lane zero.
    if constexpr (std::is_same_v<Lane, std::uint8_t>)
        return _mm_unpacklo_epi8(b, a);
    else if constexpr (std::is_same_v<Lane, std::uint16_t>)
        return _mm_unpacklo_epi16(b, a);
    else
        return _mm_unpacklo_epi32(b, a);
}

template <typename Lane>
__m128i refUnpackHighReversed(__m128i a, __m128i b) noexcept
{
    if constexpr (std::is_same_v<Lane, std::uint8_t>)
        return _mm_unpackhi_epi8(b, a);
    else if constexpr (std::is_same_v<Lane, std::uint16_t>)
        return _mm_unpackhi_epi16(b, a);
    else
        return _mm_unpackhi_epi32(b, a);
}

template <typename Lane>
void checkIntegerVectorPair(const Vector128 &a, const Vector128 &b) noexcept
{
    const __m128i na = toNative(a);
    const __m128i nb = toNative(b);
    const __m128i greater = refGreaterSigned<Lane>(na, nb);
    const __m128i zero = _mm_setzero_si128();
    const __m128i sign = refGreaterSigned<Lane>(zero, na);
    __m128i absolute{};
    if constexpr (std::is_same_v<Lane, std::uint8_t>)
        absolute = _mm_sub_epi8(_mm_xor_si128(na, sign), sign);
    else if constexpr (std::is_same_v<Lane, std::uint16_t>)
        absolute = _mm_sub_epi16(_mm_xor_si128(na, sign), sign);
    else
        absolute = _mm_sub_epi32(_mm_xor_si128(na, sign), sign);

    expectEqual(addLanes<Lane>(a, b), refAdd<Lane>(na, nb));
    expectEqual(subtractLanes<Lane>(a, b), refSubtract<Lane>(na, nb));
    expectEqual(compareEqualLanes<Lane>(a, b), refEqual<Lane>(na, nb));
    expectEqual(compareGreaterSigned<Lane>(a, b), greater);

    const __m128i minimum = _mm_or_si128(_mm_and_si128(greater, nb),
                                         _mm_andnot_si128(greater, na));
    const __m128i maximum = _mm_or_si128(_mm_and_si128(greater, na),
                                         _mm_andnot_si128(greater, nb));
    expectEqual(minSigned<Lane>(a, b), minimum);
    expectEqual(maxSigned<Lane>(a, b), maximum);
    expectEqual(absoluteSigned<Lane>(a), absolute);
    if constexpr (std::is_same_v<Lane, std::uint8_t>)
    {
        expectEqual(addSaturatingSignedLanes<Lane>(a, b), _mm_adds_epi8(na, nb));
        expectEqual(subtractSaturatingSignedLanes<Lane>(a, b), _mm_subs_epi8(na, nb));
        expectEqual(addSaturatingUnsignedLanes<Lane>(a, b), _mm_adds_epu8(na, nb));
        expectEqual(subtractSaturatingUnsignedLanes<Lane>(a, b), _mm_subs_epu8(na, nb));
    }
    else if constexpr (std::is_same_v<Lane, std::uint16_t>)
    {
        expectEqual(addSaturatingSignedLanes<Lane>(a, b), _mm_adds_epi16(na, nb));
        expectEqual(subtractSaturatingSignedLanes<Lane>(a, b), _mm_subs_epi16(na, nb));
    }
    if constexpr (std::is_same_v<Lane, std::uint32_t>)
    {
        const __m128i signBias = _mm_set1_epi32(INT32_MIN);
        const __m128i sum = _mm_add_epi32(na, nb);
        const __m128i overflow = _mm_cmpgt_epi32(_mm_xor_si128(na, signBias),
                                                  _mm_xor_si128(sum, signBias));
        const __m128i unsignedSum = _mm_or_si128(sum, overflow);
        const __m128i difference = _mm_sub_epi32(na, nb);
        const __m128i underflow = _mm_cmpgt_epi32(_mm_xor_si128(nb, signBias),
                                                   _mm_xor_si128(na, signBias));
        const __m128i unsignedDifference = _mm_andnot_si128(underflow, difference);
        expectEqual(addSaturatingUnsignedLanes<Lane>(a, b), unsignedSum);
        expectEqual(subtractSaturatingUnsignedLanes<Lane>(a, b), unsignedDifference);
    }
    expectEqual(interleaveLow<Lane>(a, b), refUnpackLowReversed<Lane>(na, nb));
    expectEqual(interleaveHigh<Lane>(a, b), refUnpackHighReversed<Lane>(na, nb));
}
} // namespace

int main()
{
    std::uint32_t state = 0x6D4B79F5u;
    for (std::uint32_t iteration = 0; iteration < 4096u; ++iteration)
    {
        Vector128 a{};
        Vector128 b{};
        for (std::size_t i = 0; i < a.bytes.size(); ++i)
        {
            state = state * 1664525u + 1013904223u;
            a.bytes[i] = static_cast<std::uint8_t>(state >> 24u);
            state = state * 1664525u + 1013904223u;
            b.bytes[i] = static_cast<std::uint8_t>(state >> 24u);
        }

        checkIntegerVectorPair<std::uint8_t>(a, b);
        checkIntegerVectorPair<std::uint16_t>(a, b);
        checkIntegerVectorPair<std::uint32_t>(a, b);
        expectEqual(packSigned32To16(a, b),
                    _mm_packs_epi32(toNative(a), toNative(b)));
    }

    return 0;
}
