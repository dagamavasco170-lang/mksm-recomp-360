#include "ps2_recompiled_functions.h"
#include "mksm_generated_probe_entry.h"
#include "ps2_runtime_macros.h"
#include "ps2_recomp_generated_mmi_probe.h"

#include <cstdint>
#include <cstdio>
#include <vector>

int main()
{
    constexpr std::uint32_t returnPc = 0xCAFE1234u;
    std::vector<std::uint8_t> rdram(PS2_RAM_SIZE, 0u);
    R5900Context context{};
    R5900Context *ctx = &context;
    SET_GPR_U32(ctx, 31, returnPc);
    SET_GPR_U32(ctx, 8, 0x01FFFFF8u);
    SET_GPR_U32(ctx, 20, 0x20000000u);
    SET_GPR_U32(ctx, 9, 0xA1B2C3D4u);
    SET_GPR_U64(ctx, 14, 0x0123456789ABCDEFull);
    SET_GPR_U32(ctx, 16, 0x5Au);
    SET_GPR_U32(ctx, 18, 0xBEEFu);

    MKSM_RECOMP_GENERATED_PROBE_ENTRY(rdram.data(), ctx, nullptr);

    std::uint32_t failures = 0u;
    const std::uint32_t r1 = GPR_U32(ctx, 1);
    const std::uint32_t r2 = GPR_U32(ctx, 2);
    const std::uint32_t r3 = GPR_U32(ctx, 3);
    const std::uint32_t r4 = GPR_U32(ctx, 4);
    const std::uint32_t r5 = GPR_U32(ctx, 5);
    const std::uint32_t r6 = GPR_U32(ctx, 6);
    const std::uint32_t r7 = GPR_U32(ctx, 7);

    if (r1 != 0x12345678u || r2 != 0x12345677u || r3 != 0x12345688u)
        failures |= 1u << 0u;
    if (r4 != 0x12345688u || r5 != 0x2468AD10u)
        failures |= 1u << 1u;
    if (r6 != 9u || r7 != 12u)
        failures |= 1u << 2u;
    if (context.pc != returnPc || context.in_delay_slot)
        failures |= 1u << 3u;

    const std::uint64_t r12 = GPR_U64(ctx, 12);
    const std::uint64_t r15 = GPR_U64(ctx, 15);
    if (GPR_U32(ctx, 10) != 0xA1B2C3D4u)
        failures |= 1u << 4u;
    if (GPR_U32(ctx, 11) != 0xC3u || r12 != 0xFFFFFFFFFFFFFFA1ull ||
        GPR_U32(ctx, 13) != 0xA1B2u)
        failures |= 1u << 5u;
    if (r15 != 0x0123456789ABCDEFull || GPR_U32(ctx, 17) != 0x5Au ||
        GPR_U32(ctx, 19) != 0xBEEFu)
        failures |= 1u << 6u;

    const std::uint32_t mask = PS2_RAM_MASK;
    if (rdram[mask - 7u] != 0xD4u ||
        rdram[mask - 6u] != 0xC3u ||
        rdram[mask - 5u] != 0xB2u ||
        rdram[mask - 4u] != 0xA1u ||
        rdram[0] != 0xEFu || rdram[1] != 0xCDu ||
        rdram[2] != 0xABu || rdram[3] != 0x89u ||
        rdram[4] != 0x67u || rdram[5] != 0x45u ||
        rdram[6] != 0x23u || rdram[7] != 0x5Au ||
        rdram[8] != 0xEFu || rdram[9] != 0xBEu)
        failures |= 1u << 7u;

    std::printf("MKSM_RECOMP_TRACE failures=%08X pc=%08X r1=%08X r2=%08X "
                "r3=%08X r4=%08X r5=%08X r6=%08X r7=%08X "
                "lw=%08X lbu=%08X lb=%016llX lhu=%04X ld=%016llX\n",
                static_cast<unsigned>(failures),
                static_cast<unsigned>(context.pc),
                static_cast<unsigned>(r1), static_cast<unsigned>(r2),
                static_cast<unsigned>(r3), static_cast<unsigned>(r4),
                static_cast<unsigned>(r5), static_cast<unsigned>(r6),
                static_cast<unsigned>(r7),
                static_cast<unsigned>(GPR_U32(ctx, 10)),
                static_cast<unsigned>(GPR_U32(ctx, 11)),
                static_cast<unsigned long long>(r12),
                static_cast<unsigned>(GPR_U32(ctx, 13)),
                static_cast<unsigned long long>(r15));

    const auto mmi = runGeneratedMmiProbe(rdram.data());
    std::printf("MKSM_RECOMP_MMI_TRACE failures=%08X pc=%08X "
                "pmfhl=%08X/%08X/%08X/%08X "
                "dest=%08X/%08X/%08X/%08X hi=%08X/%08X/%08X/%08X "
                "lo=%08X/%08X/%08X/%08X "
                "subdest=%08X/%08X/%08X/%08X "
                "subhi=%08X/%08X/%08X/%08X sublo=%08X/%08X/%08X/%08X "
                "pmthl_hi=%08X/%08X/%08X/%08X pmthl_lo=%08X/%08X/%08X/%08X\n",
                static_cast<unsigned>(mmi.failures),
                static_cast<unsigned>(mmi.pc),
                static_cast<unsigned>(mmi.pmfhlLw[0]),
                static_cast<unsigned>(mmi.pmfhlLw[1]),
                static_cast<unsigned>(mmi.pmfhlLw[2]),
                static_cast<unsigned>(mmi.pmfhlLw[3]),
                static_cast<unsigned>(mmi.destination[0]),
                static_cast<unsigned>(mmi.destination[1]),
                static_cast<unsigned>(mmi.destination[2]),
                static_cast<unsigned>(mmi.destination[3]),
                static_cast<unsigned>(mmi.hi[0]),
                static_cast<unsigned>(mmi.hi[1]),
                static_cast<unsigned>(mmi.hi[2]),
                static_cast<unsigned>(mmi.hi[3]),
                static_cast<unsigned>(mmi.lo[0]),
                static_cast<unsigned>(mmi.lo[1]),
                static_cast<unsigned>(mmi.lo[2]),
                static_cast<unsigned>(mmi.lo[3]),
                static_cast<unsigned>(mmi.subDestination[0]),
                static_cast<unsigned>(mmi.subDestination[1]),
                static_cast<unsigned>(mmi.subDestination[2]),
                static_cast<unsigned>(mmi.subDestination[3]),
                static_cast<unsigned>(mmi.subHi[0]),
                static_cast<unsigned>(mmi.subHi[1]),
                static_cast<unsigned>(mmi.subHi[2]),
                static_cast<unsigned>(mmi.subHi[3]),
                static_cast<unsigned>(mmi.subLo[0]),
                static_cast<unsigned>(mmi.subLo[1]),
                static_cast<unsigned>(mmi.subLo[2]),
                static_cast<unsigned>(mmi.subLo[3]),
                static_cast<unsigned>(mmi.pmthlHi[0]),
                static_cast<unsigned>(mmi.pmthlHi[1]),
                static_cast<unsigned>(mmi.pmthlHi[2]),
                static_cast<unsigned>(mmi.pmthlHi[3]),
                static_cast<unsigned>(mmi.pmthlLo[0]),
                static_cast<unsigned>(mmi.pmthlLo[1]),
                static_cast<unsigned>(mmi.pmthlLo[2]),
                static_cast<unsigned>(mmi.pmthlLo[3]));
    return (failures == 0u && mmi.failures == 0u) ? 0 : 1;
}
