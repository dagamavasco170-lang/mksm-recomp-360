#include "ps2_recompiled_functions.h"
#include "mksm_generated_probe_entry.h"
#include "ps2_runtime_macros.h"
#include "ps2_recomp_generated_mmi_probe.h"

#include <array>
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

    // Execute the generated quadword function on the desktop runtime too.
    // The effective address is intentionally unaligned and must round down.
    constexpr std::uint64_t quadLow = 0x0706050403020100ull;
    constexpr std::uint64_t quadHigh = 0x0F0E0D0C0B0A0908ull;
    R5900Context quadContext{};
    R5900Context *quadCtx = &quadContext;
    SET_GPR_U32(quadCtx, 31, returnPc);
    SET_GPR_U32(quadCtx, 1, 0x2000105Bu);
    SET_GPR_VEC(quadCtx, 2, _mm_set_epi64x(
        static_cast<long long>(quadHigh), static_cast<long long>(quadLow)));
    mksm_ee_lq_sq_probe_0x100400(rdram.data(), quadCtx, nullptr);
    bool quadFailures = quadContext.pc != returnPc || quadContext.in_delay_slot;
    quadFailures = quadFailures ||
        static_cast<std::uint64_t>(PS2_EXTRACT_EPI64_0(quadContext.r[3])) != quadLow ||
        static_cast<std::uint64_t>(PS2_EXTRACT_EPI64_1(quadContext.r[3])) != quadHigh;
    for (std::uint32_t i = 0; i < 16u; ++i) {
        quadFailures = quadFailures || rdram[0x1050u + i] != i ||
                       rdram[0x1060u + i] != 0u;
    }
    std::printf("MKSM_RECOMP_LQ_SQ_TRACE failures=%08X\n",
                static_cast<unsigned>(quadFailures ? 1u : 0u));
    if (quadFailures)
        failures |= 1u << 8u;

    // Execute PS2Recomp-generated LWL/LWR/SWL/SWR on the desktop reference
    // runtime for every byte offset using separate store words per operation.
    constexpr std::uint32_t mergeReturnPc = 0xCAFE5678u;
    constexpr std::array<std::uint32_t, 4> expectedLwl{
        0x11BBCCDDu, 0x2211CCDDu, 0x332211DDu, 0xF2332211u};
    constexpr std::array<std::uint32_t, 4> expectedLwr{
        0xF2332211u, 0xAAF23322u, 0xAABBF233u, 0xAABBCCF2u};
    constexpr std::array<std::uint32_t, 4> expectedSwl{
        0x443322AAu, 0x4433AABBu, 0x44AABBCCu, 0xAABBCCDDu};
    constexpr std::array<std::uint32_t, 4> expectedSwr{
        0xAABBCCDDu, 0xBBCCDD11u, 0xCCDD2211u, 0xDD332211u};
    rdram[0x1080u] = 0x11u; rdram[0x1081u] = 0x22u;
    rdram[0x1082u] = 0x33u; rdram[0x1083u] = 0xF2u;
    for (std::uint32_t i = 0; i < 4u; ++i) {
        for (std::uint32_t b = 0; b < 4u; ++b) {
            rdram[0x10A0u + i * 4u + b] =
                static_cast<std::uint8_t>(0x44332211u >> (b * 8u));
            rdram[0x10C0u + i * 4u + b] =
                static_cast<std::uint8_t>(0x44332211u >> (b * 8u));
        }
    }
    R5900Context mergeContext{};
    R5900Context *mergeCtx = &mergeContext;
    SET_GPR_U32(mergeCtx, 31, mergeReturnPc);
    SET_GPR_U32(mergeCtx, 2, 0xAABBCCDDu);
    for (std::uint32_t i = 0; i < 4u; ++i) {
        SET_GPR_U32(mergeCtx, 4u + i, 0x20001080u + i);
        SET_GPR_U32(mergeCtx, 8u + i, 0x20001080u + i);
        SET_GPR_U32(mergeCtx, 12u + i, 0x200010A0u + i * 4u + i);
        SET_GPR_U32(mergeCtx, 16u + i, 0x200010C0u + i * 4u + i);
        SET_GPR_VEC(mergeCtx, 20u + i, _mm_set_epi64x(
            static_cast<long long>(0x5060708010203040ull),
            static_cast<long long>(0x55667788AABBCCDDull)));
        SET_GPR_VEC(mergeCtx, 24u + i, _mm_set_epi64x(
            static_cast<long long>(0x5060708010203040ull),
            static_cast<long long>(0x55667788AABBCCDDull)));
    }
    mksm_ee_unaligned_merge_probe_0x100500(rdram.data(), mergeCtx, nullptr);
    bool mergeFailed = mergeContext.pc != mergeReturnPc ||
                       mergeContext.in_delay_slot;
    for (std::uint32_t i = 0; i < 4u; ++i) {
        const std::uint64_t lwlLow = GPR_U64(mergeCtx, 20u + i);
        const std::uint64_t lwrLow = GPR_U64(mergeCtx, 24u + i);
        const std::uint32_t expectedLwlUpper = i == 3u ? 0xFFFFFFFFu : 0u;
        const std::uint32_t expectedLwrUpper =
            i == 0u ? 0xFFFFFFFFu : 0x55667788u;
        mergeFailed = mergeFailed ||
            static_cast<std::uint32_t>(lwlLow) != expectedLwl[i] ||
            static_cast<std::uint32_t>(lwlLow >> 32u) != expectedLwlUpper ||
            static_cast<std::uint32_t>(lwrLow) != expectedLwr[i] ||
            static_cast<std::uint32_t>(lwrLow >> 32u) != expectedLwrUpper ||
            static_cast<std::uint64_t>(PS2_EXTRACT_EPI64_1(mergeContext.r[20u + i])) !=
                0x5060708010203040ull ||
            static_cast<std::uint64_t>(PS2_EXTRACT_EPI64_1(mergeContext.r[24u + i])) !=
                0x5060708010203040ull;
        std::uint32_t swlWord{}, swrWord{};
        for (std::uint32_t b = 0; b < 4u; ++b) {
            swlWord |= static_cast<std::uint32_t>(rdram[0x10A0u + i * 4u + b]) << (b * 8u);
            swrWord |= static_cast<std::uint32_t>(rdram[0x10C0u + i * 4u + b]) << (b * 8u);
        }
        mergeFailed = mergeFailed || swlWord != expectedSwl[i] ||
                      swrWord != expectedSwr[i];
    }
    std::printf("MKSM_RECOMP_UNALIGNED_MERGE_TRACE failures=%08X\n",
                static_cast<unsigned>(mergeFailed ? 1u : 0u));
    if (mergeFailed)
        failures |= 1u << 9u;

    // Execute generated arithmetic independently against hand-calculated reference values.
    constexpr std::uint32_t arithmeticReturnPc = 0xCAFE7890u;
    R5900Context arithmeticContext{};
    R5900Context *arithmeticCtx = &arithmeticContext;
    SET_GPR_U32(arithmeticCtx, 31, arithmeticReturnPc);
    SET_GPR_U32(arithmeticCtx, 1, 0xFFFFFFF9u); // -7
    SET_GPR_U32(arithmeticCtx, 2, 3u);
    SET_GPR_U32(arithmeticCtx, 5, 0xFFFFFFFFu);
    SET_GPR_U32(arithmeticCtx, 6, 2u);
    SET_GPR_U32(arithmeticCtx, 9, 0xFFFFFFF7u); // -9, divisor-zero signed case
    SET_GPR_U32(arithmeticCtx, 10, 0xFFFFFFFFu); // -1 for signed-overflow case
    SET_GPR_U32(arithmeticCtx, 13, 0xFFFFFFFFu);
    SET_GPR_U32(arithmeticCtx, 23, 0x80000000u); // INT32_MIN
    SET_GPR_U32(arithmeticCtx, 24, 0xFFFFFFFFu); // -1
    SET_GPR_U32(arithmeticCtx, 25, 0xFFFFFFF9u); // -7
    SET_GPR_U32(arithmeticCtx, 26, 3u);
    SET_GPR_U32(arithmeticCtx, 14, 3u);
    MKSM_RECOMP_GENERATED_ARITHMETIC_ENTRY(rdram.data(), arithmeticCtx, nullptr);
    const bool arithmeticFailed = arithmeticContext.pc != arithmeticReturnPc ||
        arithmeticContext.in_delay_slot ||
        GPR_U64(arithmeticCtx, 3) != 0xFFFFFFFFFFFFFFEBull ||
        GPR_U64(arithmeticCtx, 4) != 0u ||
        GPR_U64(arithmeticCtx, 7) != 0xFFFFFFFFFFFFFFFEull ||
        GPR_U64(arithmeticCtx, 8) != 1u ||
        GPR_U64(arithmeticCtx, 11) != 0xFFFFFFFFFFFFFFFEull ||
        GPR_U64(arithmeticCtx, 12) != 0xFFFFFFFFFFFFFFFFull ||
        GPR_U64(arithmeticCtx, 15) != 0x55555555ull ||
        GPR_U64(arithmeticCtx, 16) != 0u ||
        GPR_U64(arithmeticCtx, 17) != 1u ||
        GPR_U64(arithmeticCtx, 18) != 0xFFFFFFFFFFFFFFF7ull ||
        GPR_U64(arithmeticCtx, 19) != 0xFFFFFFFFFFFFFFFFull ||
        GPR_U64(arithmeticCtx, 20) != 0xFFFFFFFFFFFFFFFFull ||
        GPR_U64(arithmeticCtx, 21) != 0xFFFFFFFF80000000ull ||
        GPR_U64(arithmeticCtx, 22) != 0u;
    std::printf("MKSM_RECOMP_ARITHMETIC_TRACE failures=%08X pc=%08X "
                "mult=%016llX/%016llX multu=%016llX/%016llX "
                "div=%016llX/%016llX divu=%016llX/%016llX edges=%016llX/%016llX,%016llX/%016llX,%016llX/%016llX\n",
                static_cast<unsigned>(arithmeticFailed ? 1u : 0u),
                static_cast<unsigned>(arithmeticContext.pc),
                static_cast<unsigned long long>(GPR_U64(arithmeticCtx, 3)),
                static_cast<unsigned long long>(GPR_U64(arithmeticCtx, 4)),
                static_cast<unsigned long long>(GPR_U64(arithmeticCtx, 7)),
                static_cast<unsigned long long>(GPR_U64(arithmeticCtx, 8)),
                static_cast<unsigned long long>(GPR_U64(arithmeticCtx, 11)),
                static_cast<unsigned long long>(GPR_U64(arithmeticCtx, 12)),
                static_cast<unsigned long long>(GPR_U64(arithmeticCtx, 15)),
                static_cast<unsigned long long>(GPR_U64(arithmeticCtx, 16)),
                static_cast<unsigned long long>(GPR_U64(arithmeticCtx, 17)),
                static_cast<unsigned long long>(GPR_U64(arithmeticCtx, 18)),
                static_cast<unsigned long long>(GPR_U64(arithmeticCtx, 19)),
                static_cast<unsigned long long>(GPR_U64(arithmeticCtx, 20)),
                static_cast<unsigned long long>(GPR_U64(arithmeticCtx, 21)),
                static_cast<unsigned long long>(GPR_U64(arithmeticCtx, 22)));
    if (arithmeticFailed)
        failures |= 1u << 10u;

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
