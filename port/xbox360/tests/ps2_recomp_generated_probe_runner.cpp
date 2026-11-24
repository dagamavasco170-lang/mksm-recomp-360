#include "ps2_recompiled_functions.h"
#include "ps2_runtime_macros.h"

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
    SET_GPR_U32(ctx, 8, 0x01FFFFFFu);
    SET_GPR_U32(ctx, 9, 0xA1B2C3D4u);
    SET_GPR_U64(ctx, 14, 0x0123456789ABCDEFull);
    SET_GPR_U32(ctx, 16, 0x5Au);
    SET_GPR_U32(ctx, 18, 0xBEEFu);

    mksm_ee_codegen_probe_0x100000(rdram.data(), ctx, nullptr);

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
    if (rdram[PS2_RAM_MASK] != 0xD4u || rdram[0] != 0xC3u ||
        rdram[1] != 0xB2u || rdram[2] != 0xA1u ||
        rdram[7] != 0xEFu || rdram[8] != 0xCDu ||
        rdram[13] != 0x23u || rdram[14] != 0x5Au ||
        rdram[15] != 0xEFu || rdram[16] != 0xBEu)
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
    return failures == 0u ? 0 : 1;
}
