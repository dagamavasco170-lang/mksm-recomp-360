#pragma once

#include <cstdint>
#include <stdexcept>

namespace ps2::xbox360::rdram_address
{
// EE address-space classification is deliberately separate from device
// emulation. Only RegionKind::Rdram is currently backed by the generated ABI.
enum class RegionKind : std::uint8_t
{
    Rdram,
    Scratchpad,
    EeIo,
    Vu0Code,
    Vu0Data,
    Vu1Code,
    Vu1Data,
    GsPrivileged,
    IopRam,
    Bios,
    TlbDependent,
    Unmapped
};

// Documented retail EE memory-window sizes.
inline constexpr std::uint32_t kRdramSize = 32u * 1024u * 1024u;
inline constexpr std::uint32_t kRdramLast = kRdramSize - 1u;
inline constexpr std::uint32_t kUncachedRdramBase = 0x20000000u;
inline constexpr std::uint32_t kUncachedRdramLast = 0x21FFFFFFu;
inline constexpr std::uint32_t kAcceleratedRdramBase = 0x30100000u;
inline constexpr std::uint32_t kAcceleratedRdramLast = 0x31FFFFFFu;

inline constexpr std::uint32_t kEeIoBase = 0x10000000u;
inline constexpr std::uint32_t kEeIoSize = 64u * 1024u;
inline constexpr std::uint32_t kVu0CodeBase = 0x11000000u;
inline constexpr std::uint32_t kVu0CodeSize = 4u * 1024u;
inline constexpr std::uint32_t kVu0DataBase = 0x11004000u;
inline constexpr std::uint32_t kVu0DataSize = 4u * 1024u;
inline constexpr std::uint32_t kVu1CodeBase = 0x11008000u;
inline constexpr std::uint32_t kVu1CodeSize = 16u * 1024u;
inline constexpr std::uint32_t kVu1DataBase = 0x1100C000u;
inline constexpr std::uint32_t kVu1DataSize = 16u * 1024u;
inline constexpr std::uint32_t kGsPrivilegedBase = 0x12000000u;
inline constexpr std::uint32_t kGsPrivilegedSize = 8u * 1024u;
inline constexpr std::uint32_t kIopRamBase = 0x1C000000u;
inline constexpr std::uint32_t kIopRamSize = 2u * 1024u * 1024u;
inline constexpr std::uint32_t kBiosBase = 0x1FC00000u;
inline constexpr std::uint32_t kBiosSize = 4u * 1024u * 1024u;
inline constexpr std::uint32_t kScratchpadBase = 0x70000000u;
inline constexpr std::uint32_t kScratchpadAliasBase = 0xF0000000u;
// Compatibility alias recognized by the desktop PS2Memory helpers when bit 31
// marks a scratchpad pointer (e.g. a DMA/guest-pointer selector).
inline constexpr std::uint32_t kScratchpadSize = 16u * 1024u;

namespace detail
{
[[nodiscard]] constexpr bool inRange(std::uint32_t address,
                                     std::uint32_t base,
                                     std::uint32_t size) noexcept
{
    return address >= base && (address - base) < size;
}

// Classify the documented physical apertures used by EE KSEG0/KSEG1 too.
// Scratchpad is intentionally absent: it is a separate virtual-only window.
[[nodiscard]] constexpr RegionKind classifyPhysical(std::uint32_t address) noexcept
{
    if (address < kRdramSize)
        return RegionKind::Rdram;
    if (inRange(address, kEeIoBase, kEeIoSize))
        return RegionKind::EeIo;
    if (inRange(address, kVu0CodeBase, kVu0CodeSize))
        return RegionKind::Vu0Code;
    if (inRange(address, kVu0DataBase, kVu0DataSize))
        return RegionKind::Vu0Data;
    if (inRange(address, kVu1CodeBase, kVu1CodeSize))
        return RegionKind::Vu1Code;
    if (inRange(address, kVu1DataBase, kVu1DataSize))
        return RegionKind::Vu1Data;
    if (inRange(address, kGsPrivilegedBase, kGsPrivilegedSize))
        return RegionKind::GsPrivileged;
    if (inRange(address, kIopRamBase, kIopRamSize))
        return RegionKind::IopRam;
    if (inRange(address, kBiosBase, kBiosSize))
        return RegionKind::Bios;
    return RegionKind::Unmapped;
}
} // namespace detail

// Return the EE region represented by an address without translating general
// TLB mappings or claiming that the selected device has a working handler.
[[nodiscard]] constexpr RegionKind classifyRegion(std::uint32_t address) noexcept
{
    if (detail::inRange(address, kScratchpadBase, kScratchpadSize) ||
        detail::inRange(address, kScratchpadAliasBase, kScratchpadSize))
        return RegionKind::Scratchpad;

    if (address <= kRdramLast ||
        (address >= kUncachedRdramBase && address <= kUncachedRdramLast) ||
        (address >= kAcceleratedRdramBase && address <= kAcceleratedRdramLast))
        return RegionKind::Rdram;

    // KSEG0/KSEG1 are direct-mapped; their low 29 bits select the physical
    // aperture. The KSEG2/KSEG3 space is TLB/privileged-mapping dependent.
    if (address >= 0x80000000u && address < 0xC0000000u)
        return detail::classifyPhysical(address & 0x1FFFFFFFu);
    if (address >= 0xC0000000u)
        return RegionKind::TlbDependent;

    const RegionKind physicalRegion = detail::classifyPhysical(address);
    if (physicalRegion != RegionKind::Unmapped)
        return physicalRegion;

    // The remaining KUSEG addresses may be backed by a configured TLB entry;
    // this bootstrap runtime deliberately does not guess that translation.
    if (address < 0x80000000u)
        return RegionKind::TlbDependent;

    return RegionKind::Unmapped;
}

[[nodiscard]] constexpr std::uint32_t scratchpadOffset(std::uint32_t address) noexcept
{
    const std::uint32_t base =
        address >= kScratchpadAliasBase ? kScratchpadAliasBase : kScratchpadBase;
    return address - base;
}

[[nodiscard]] inline std::uint32_t resolveOffset(std::uint32_t address)
{
    const RegionKind region = classifyRegion(address);
    switch (region)
    {
    case RegionKind::Rdram:
        break;
    case RegionKind::Scratchpad:
        throw std::out_of_range(
            "PS2 generated ABI: scratchpad memory is classified but not implemented");
    case RegionKind::EeIo:
        throw std::out_of_range(
            "PS2 generated ABI: EE I/O registers are classified but not implemented");
    case RegionKind::Vu0Code:
    case RegionKind::Vu0Data:
    case RegionKind::Vu1Code:
    case RegionKind::Vu1Data:
        throw std::out_of_range(
            "PS2 generated ABI: VU memory is classified but not implemented");
    case RegionKind::GsPrivileged:
        throw std::out_of_range(
            "PS2 generated ABI: GS registers are classified but not implemented");
    case RegionKind::IopRam:
        throw std::out_of_range(
            "PS2 generated ABI: IOP RAM is classified but not implemented");
    case RegionKind::Bios:
        throw std::out_of_range(
            "PS2 generated ABI: BIOS memory is classified but not implemented");
    case RegionKind::TlbDependent:
        throw std::out_of_range(
            "PS2 generated ABI: address requires unsupported EE TLB translation");
    case RegionKind::Unmapped:
        throw std::out_of_range(
            "PS2 generated ABI: address is in an unmapped EE region");
    }

    if (address <= kRdramLast)
        return address;

    if (address >= kUncachedRdramBase &&
        address <= kUncachedRdramLast)
        return address - kUncachedRdramBase;

    if (address >= kAcceleratedRdramBase &&
        address <= kAcceleratedRdramLast)
        return address - 0x30000000u;

    // Only the first 32 MiB physical RDRAM aperture is accepted here.
    return address & 0x1FFFFFFFu;
}
} // namespace ps2::xbox360::rdram_address
