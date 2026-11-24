#include "ps2_rdram_address.h"

#include <cstddef>
#include <cstdint>
#include <cstdio>

namespace
{
using ps2::xbox360::rdram_address::RegionKind;

struct RegionCase
{
    std::uint32_t address;
    RegionKind expected;
};

constexpr RegionCase kCases[] = {
    {0x00000000u, RegionKind::Rdram},
    {0x01FFFFFFu, RegionKind::Rdram},
    {0x20000000u, RegionKind::Rdram},
    {0x21FFFFFFu, RegionKind::Rdram},
    {0x30100000u, RegionKind::Rdram},
    {0x31FFFFFFu, RegionKind::Rdram},
    {0x80000000u, RegionKind::Rdram},
    {0x81FFFFFFu, RegionKind::Rdram},
    {0xA0100000u, RegionKind::Rdram},
    {0x10000000u, RegionKind::EeIo},
    {0x1000FFFFu, RegionKind::EeIo},
    {0x90000000u, RegionKind::EeIo},
    {0x11000000u, RegionKind::Vu0Code},
    {0x11000FFFu, RegionKind::Vu0Code},
    {0x91000000u, RegionKind::Vu0Code},
    {0x11004000u, RegionKind::Vu0Data},
    {0x11004FFFu, RegionKind::Vu0Data},
    {0x91004000u, RegionKind::Vu0Data},
    {0x11008000u, RegionKind::Vu1Code},
    {0x1100BFFFu, RegionKind::Vu1Code},
    {0x91008000u, RegionKind::Vu1Code},
    {0x1100C000u, RegionKind::Vu1Data},
    {0x1100FFFFu, RegionKind::Vu1Data},
    {0x9100C000u, RegionKind::Vu1Data},
    {0x12000000u, RegionKind::GsPrivileged},
    {0x12001FFFu, RegionKind::GsPrivileged},
    {0x92000000u, RegionKind::GsPrivileged},
    {0x1C000000u, RegionKind::IopRam},
    {0x1C1FFFFFu, RegionKind::IopRam},
    {0x9C000000u, RegionKind::IopRam},
    {0x1FC00000u, RegionKind::Bios},
    {0x1FFFFFFFu, RegionKind::Bios},
    {0x9FC00000u, RegionKind::Bios},
    {0xBFC00000u, RegionKind::Bios},
    {0x70000000u, RegionKind::Scratchpad},
    {0x70003FFFu, RegionKind::Scratchpad},
    {0x02000000u, RegionKind::TlbDependent},
    {0x22000000u, RegionKind::TlbDependent},
    {0x32000000u, RegionKind::TlbDependent},
    {0x40000000u, RegionKind::TlbDependent},
    {0xC0000000u, RegionKind::TlbDependent},
    {0xE0000000u, RegionKind::TlbDependent},
    {0xFFFFFFFFu, RegionKind::TlbDependent},
    {0x82000000u, RegionKind::Unmapped},
    {0xA2000000u, RegionKind::Unmapped},
};
} // namespace

int main()
{
    std::uint32_t failures = 0u;
    for (const RegionCase &test : kCases)
    {
        const RegionKind actual =
            ps2::xbox360::rdram_address::classifyRegion(test.address);
        if (actual != test.expected)
        {
            ++failures;
            std::printf("FAIL address=%08X expected=%u actual=%u\n",
                        static_cast<unsigned>(test.address),
                        static_cast<unsigned>(test.expected),
                        static_cast<unsigned>(actual));
        }
    }

    std::printf("MKSM_EE_MEMORY_REGION_TEST cases=%zu failures=%u\n",
                sizeof(kCases) / sizeof(kCases[0]),
                static_cast<unsigned>(failures));
    return failures == 0u ? 0 : 1;
}
