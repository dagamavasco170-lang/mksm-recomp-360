#include "ps2_rdram_address.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <stdexcept>
#include <utility>

namespace
{
using Mapping = std::pair<std::uint32_t, std::uint32_t>;

constexpr std::array<Mapping, 14> kValidMappings{{
    {0x00000000u, 0x00000000u},
    {0x00100000u, 0x00100000u},
    {0x01FFFFFCu, 0x01FFFFFCu},
    {0x01FFFFFFu, 0x01FFFFFFu},
    {0x20000000u, 0x00000000u},
    {0x20100000u, 0x00100000u},
    {0x21FFFFFCu, 0x01FFFFFCu},
    {0x21FFFFFFu, 0x01FFFFFFu},
    {0x30100000u, 0x00100000u},
    {0x31FFFFFCu, 0x01FFFFFCu},
    {0x31FFFFFFu, 0x01FFFFFFu},
    {0x80000000u, 0x00000000u},
    {0x81000000u, 0x01000000u},
    {0xA0100000u, 0x00100000u},
}};

constexpr std::array<std::uint32_t, 26> kRejectedAddresses{{
    0x02000000u, // Not a documented virtual RDRAM window.
    0x10000000u, // EE hardware registers.
    0x10010000u, // Other addresses in the non-RDRAM device/reserved area.
    0x11000000u, // VU0 code.
    0x11004000u, // VU0 data.
    0x11008000u, // VU1 code.
    0x1100C000u, // VU1 data.
    0x12000000u, // GS privileged registers.
    0x1C000000u, // IOP RAM.
    0x1FC00000u, // BIOS.
    0x1FFFFFFFu, // End of BIOS/reserved physical window, not RDRAM.
    0x22000000u, // Past the 32 MiB uncached alias.
    0x300FFFFFu, // Before the accelerated alias.
    0x32000000u, // Past the accelerated alias.
    0x40000000u, // No direct RDRAM mapping.
    0x70000000u, // Scratchpad (separate storage, not RDRAM).
    0x70003FFFu, // Last scratchpad byte.
    0x70004000u, // Past scratchpad
    0xF0000000u, // Scratchpad pointer alias, not a RDRAM mapping
    0xF0003FFFu, // Final byte of scratchpad pointer alias.
    0x82000000u, // KSEG maps this to physical 0x02000000.
    0x9FC00000u, // Cached BIOS alias.
    0xBFC00000u, // Uncached BIOS alias.
    0xC0000000u, // KSSEG requires the TLB/privileged mapping path.
    0xE0000000u, // KSEG3 requires the TLB/privileged mapping path.
    0xFFFFFFFFu,
}};
} // namespace

int main()
{
    std::uint32_t cases = 0u;
    std::uint32_t failures = 0u;

    for (const auto &[address, expected] : kValidMappings)
    {
        ++cases;
        try
        {
            const auto actual =
                ps2::xbox360::rdram_address::resolveOffset(address);
            if (actual != expected)
            {
                ++failures;
                std::printf("FAIL valid address=%08X expected=%08X actual=%08X\n",
                            static_cast<unsigned>(address),
                            static_cast<unsigned>(expected),
                            static_cast<unsigned>(actual));
            }
        }
        catch (const std::exception &error)
        {
            ++failures;
            std::printf("FAIL valid address=%08X threw=%s\n",
                        static_cast<unsigned>(address), error.what());
        }
    }

    for (const auto address : kRejectedAddresses)
    {
        ++cases;
        bool rejected = false;
        try
        {
            (void)ps2::xbox360::rdram_address::resolveOffset(address);
        }
        catch (const std::out_of_range &)
        {
            rejected = true;
        }
        catch (const std::exception &error)
        {
            ++failures;
            std::printf("FAIL rejected address=%08X unexpected exception=%s\n",
                        static_cast<unsigned>(address), error.what());
        }

        if (!rejected)
        {
            ++failures;
            std::printf("FAIL unsupported address accepted=%08X\n",
                        static_cast<unsigned>(address));
        }
    }

    std::printf("MKSM_RDRAM_ADDRESS_TEST cases=%u failures=%u\n",
                static_cast<unsigned>(cases), static_cast<unsigned>(failures));
    return failures == 0u ? 0 : 1;
}
