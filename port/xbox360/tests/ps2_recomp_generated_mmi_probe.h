#pragma once

#include "mksm_generated_probe_entry.h"
#include "ps2_mmi_hilo_helpers.h"

#include <array>
#include <cstdint>

struct Ps2GeneratedMmiProbeResult
{
    std::uint32_t failures{};
    std::uint32_t pc{};
    std::array<std::uint32_t, 4> initialHi{};
    std::array<std::uint32_t, 4> initialLo{};
    std::array<std::uint32_t, 4> pmfhlLw{};
    std::array<std::uint32_t, 4> destination{};
    std::array<std::uint32_t, 4> hi{};
    std::array<std::uint32_t, 4> lo{};
    std::array<std::uint32_t, 4> subDestination{};
    std::array<std::uint32_t, 4> subHi{};
    std::array<std::uint32_t, 4> subLo{};
    std::array<std::uint32_t, 4> pmthlHi{};
    std::array<std::uint32_t, 4> pmthlLo{};
};

[[nodiscard]] inline Ps2GeneratedMmiProbeResult
runGeneratedMmiProbe(std::uint8_t *rdram)
{
    constexpr std::uint32_t kReturnPc = 0xCAFE5678u;
    constexpr ps2::mmi::Vector128 kInitialHi{{0x10u, 0x20u, 0x30u, 0x40u}};
    constexpr ps2::mmi::Vector128 kInitialLo{{0x100u, 0x200u, 0x300u, 0x400u}};
    constexpr ps2::mmi::Vector128 kLhs{{0x00030002u, 0x00050004u,
                                        0x00070006u, 0x00090008u}};
    constexpr ps2::mmi::Vector128 kRhs{{0x0014000Au, 0x0028001Eu,
                                        0x003C0032u, 0x00500046u}};
    constexpr ps2::mmi::Vector128 kPmthlSource{{0xDEADBEEFu, 0xCAFEBABEu,
                                                0x0BADF00Du, 0x8BADF00Du}};
    constexpr std::array<std::uint32_t, 4> kExpectedDestination{
        0x00000114u, 0x00000088u, 0x0000042Cu, 0x00000260u};
    constexpr std::array<std::uint32_t, 4> kExpectedHi{
        0x00000088u, 0x000000E8u, 0x00000260u, 0x00000310u};
    constexpr std::array<std::uint32_t, 4> kExpectedLo{
        0x00000114u, 0x0000023Cu, 0x0000042Cu, 0x000005A4u};
    constexpr std::array<std::uint32_t, 4> kExpectedSubDestination{
        0x00000100u, 0x00000010u, 0x00000300u, 0x00000030u};

    Ps2GeneratedMmiProbeResult result{};
    R5900Context context{};
    R5900Context *ctx = &context;
    SET_GPR_U32(ctx, 31u, kReturnPc);
    Ps2MmiWriteGprVector(ctx, 1u, kInitialHi);
    Ps2MmiWriteGprVector(ctx, 2u, kInitialLo);
    Ps2MmiWriteGprVector(ctx, 5u, kLhs);
    Ps2MmiWriteGprVector(ctx, 6u, kRhs);
    Ps2MmiWriteGprVector(ctx, 14u, kPmthlSource);

    MKSM_RECOMP_GENERATED_MMI_ENTRY(rdram, ctx, nullptr);

    result.pc = ctx->pc;
    result.initialHi = Ps2MmiReadGprVector(ctx, 3u).words;
    result.initialLo = Ps2MmiReadGprVector(ctx, 4u).words;
    result.pmfhlLw = Ps2MmiReadGprVector(ctx, 13u).words;
    result.destination = Ps2MmiReadGprVector(ctx, 7u).words;
    result.hi = Ps2MmiReadGprVector(ctx, 8u).words;
    result.lo = Ps2MmiReadGprVector(ctx, 9u).words;
    result.subDestination = Ps2MmiReadGprVector(ctx, 12u).words;
    result.subHi = Ps2MmiReadGprVector(ctx, 10u).words;
    result.subLo = Ps2MmiReadGprVector(ctx, 11u).words;
    result.pmthlHi = Ps2MmiReadGprVector(ctx, 15u).words;
    result.pmthlLo = Ps2MmiReadGprVector(ctx, 16u).words;

    if (result.pc != kReturnPc || ctx->in_delay_slot)
        result.failures |= 1u << 0u;
    if (result.initialHi != kInitialHi.words || result.initialLo != kInitialLo.words)
        result.failures |= 1u << 1u;
    if (result.pmfhlLw != std::array<std::uint32_t, 4>{
            0x00000100u, 0x00000010u, 0x00000300u, 0x00000030u})
        result.failures |= 1u << 7u;
    if (result.destination != kExpectedDestination)
        result.failures |= 1u << 2u;
    if (result.hi != kExpectedHi || result.lo != kExpectedLo)
        result.failures |= 1u << 3u;

    // Verify PMTHL.LW both in HI/LO and through the generated destination GPRs.
    const auto state = Ps2MmiReadHiLo(ctx);
    constexpr std::array<std::uint32_t, 4> kExpectedPmthlHi{
        0xCAFEBABEu, 0x00000020u, 0x8BADF00Du, 0x00000040u};
    constexpr std::array<std::uint32_t, 4> kExpectedPmthlLo{
        0xDEADBEEFu, 0x00000200u, 0x0BADF00Du, 0x00000400u};
    if (state.hi.words != kExpectedPmthlHi || state.lo.words != kExpectedPmthlLo)
        result.failures |= 1u << 4u;
    if (result.subDestination != kExpectedSubDestination ||
        result.subHi != kInitialHi.words || result.subLo != kInitialLo.words)
        result.failures |= 1u << 5u;
    if (result.pmthlHi != kExpectedPmthlHi ||
        result.pmthlLo != kExpectedPmthlLo)
        result.failures |= 1u << 8u;
    if (GPR_U32(ctx, 0u) != 0u)
        result.failures |= 1u << 6u;

    return result;
}
