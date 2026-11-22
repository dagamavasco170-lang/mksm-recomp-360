#pragma once

#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <span>
#include <type_traits>

namespace ps2::xbox360::guest_memory
{
// These helpers model the PS2 EE's little-endian byte layout explicitly.
// They do not map PS2 virtual addresses, I/O registers, scratchpad or VU memories.
inline bool validWrappedRam(std::span<const uint8_t> memory) noexcept
{
    return !memory.empty() &&
           memory.size() <= (uint64_t{1} << 32u) &&
           std::has_single_bit(memory.size());
}

template <typename UInt>
    requires(std::is_integral_v<UInt> && std::is_unsigned_v<UInt> &&
             (sizeof(UInt) == 1u || sizeof(UInt) == 2u ||
              sizeof(UInt) == 4u || sizeof(UInt) == 8u))
[[nodiscard]] bool readLittleEndian(std::span<const uint8_t> memory,
                                    uint32_t address,
                                    UInt &value) noexcept
{
    if (!validWrappedRam(memory))
        return false;

    const uint64_t mask = static_cast<uint64_t>(memory.size() - 1u);
    UInt decoded = 0u;
    for (size_t i = 0; i < sizeof(UInt); ++i)
    {
        const auto index = static_cast<size_t>((static_cast<uint64_t>(address) + i) & mask);
        decoded |= static_cast<UInt>(memory[index]) << (i * 8u);
    }

    value = decoded;
    return true;
}

template <typename UInt>
    requires(std::is_integral_v<UInt> && std::is_unsigned_v<UInt> &&
             (sizeof(UInt) == 1u || sizeof(UInt) == 2u ||
              sizeof(UInt) == 4u || sizeof(UInt) == 8u))
[[nodiscard]] bool writeLittleEndian(std::span<uint8_t> memory,
                                     uint32_t address,
                                     UInt value) noexcept
{
    if (memory.empty() ||
        memory.size() > (uint64_t{1} << 32u) ||
        !std::has_single_bit(memory.size()))
        return false;

    const uint64_t mask = static_cast<uint64_t>(memory.size() - 1u);
    for (size_t i = 0; i < sizeof(UInt); ++i)
    {
        const auto index = static_cast<size_t>((static_cast<uint64_t>(address) + i) & mask);
        memory[index] = static_cast<uint8_t>((value >> (i * 8u)) & UInt{0xFFu});
    }
    return true;
}

// A 128-bit guest load/store is kept as bytes so host vector lane order never
// silently changes the guest memory representation.
using Raw128 = std::array<uint8_t, 16>;

[[nodiscard]] inline bool readRaw128(std::span<const uint8_t> memory,
                                     uint32_t address,
                                     Raw128 &value) noexcept
{
    if (!validWrappedRam(memory))
        return false;

    const uint64_t mask = static_cast<uint64_t>(memory.size() - 1u);
    for (size_t i = 0; i < value.size(); ++i)
    {
        const auto index = static_cast<size_t>((static_cast<uint64_t>(address) + i) & mask);
        value[i] = memory[index];
    }
    return true;
}

[[nodiscard]] inline bool writeRaw128(std::span<uint8_t> memory,
                                      uint32_t address,
                                      const Raw128 &value) noexcept
{
    if (memory.empty() ||
        memory.size() > (uint64_t{1} << 32u) ||
        !std::has_single_bit(memory.size()))
        return false;

    const uint64_t mask = static_cast<uint64_t>(memory.size() - 1u);
    for (size_t i = 0; i < value.size(); ++i)
    {
        const auto index = static_cast<size_t>((static_cast<uint64_t>(address) + i) & mask);
        memory[index] = value[i];
    }
    return true;
}
} // namespace ps2::xbox360::guest_memory
