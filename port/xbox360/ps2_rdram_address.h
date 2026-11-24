#pragma once

#include <cstdint>
#include <stdexcept>

namespace ps2::xbox360::rdram_address
{
// The ordinary EE mappings documented for retail 32 MiB RDRAM:
//   0x00000000-0x01FFFFFF: cached/default RAM window
//   0x20000000-0x21FFFFFF: uncached RAM alias
//   0x30100000-0x31FFFFFF: uncached, accelerated RAM alias
// KSEG0/KSEG1 aliases are accepted only when they resolve into the first
// 32 MiB physical RDRAM window. This is deliberately not a general TLB.
inline constexpr std::uint32_t kRdramSize = 32u * 1024u * 1024u;
inline constexpr std::uint32_t kRdramLast = kRdramSize - 1u;

inline constexpr std::uint32_t kUncachedRdramBase = 0x20000000u;
inline constexpr std::uint32_t kUncachedRdramLast = 0x21FFFFFFu;
inline constexpr std::uint32_t kAcceleratedRdramBase = 0x30100000u;
inline constexpr std::uint32_t kAcceleratedRdramLast = 0x31FFFFFFu;

[[nodiscard]] inline std::uint32_t resolveOffset(std::uint32_t address)
{
    if (address <= kRdramLast)
        return address;

    if (address >= kUncachedRdramBase &&
        address <= kUncachedRdramLast)
        return address - kUncachedRdramBase;

    if (address >= kAcceleratedRdramBase &&
        address <= kAcceleratedRdramLast)
        return address - 0x30000000u;

    // KSEG0 (0x80000000-0x9FFFFFFF) and KSEG1
    // (0xA0000000-0xBFFFFFFF) map through the low 29 physical bits.
    // Accept only the actual RDRAM aperture, not MMIO, VU, IOP or BIOS.
    if (address >= 0x80000000u && address < 0xC0000000u)
    {
        const std::uint32_t physical = address & 0x1FFFFFFFu;
        if (physical < kRdramSize)
            return physical;

        if (physical >= 0x10000000u && physical < 0x20000000u)
            throw std::out_of_range(
                "PS2 generated ABI: KSEG address resolves to unsupported MMIO/BIOS memory");
    }

    if (address >= 0x70000000u && address < 0x70004000u)
        throw std::out_of_range(
            "PS2 generated ABI: scratchpad memory is not implemented");

    if (address >= 0x10000000u && address < 0x20000000u)
        throw std::out_of_range(
            "PS2 generated ABI: I/O, VU, GS, IOP and BIOS memory are not implemented");

    throw std::out_of_range(
        "PS2 generated ABI: address is outside supported RDRAM virtual windows");
}
} // namespace ps2::xbox360::rdram_address
