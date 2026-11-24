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
    std::array<std::uint32_t, 4> destination{};
    std::array<std::uint32_t, 4> hi{};
    std::array<std::uint32_t, 4> lo{};
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
    constexpr std::array<std::uint32_t, 4> kExpectedDestination{
        0x00000114u, 0x00000088u, 0x0000042Cu, 0x00000260u};
    constexpr std::array<std::uint32_t, 4> kExpectedHi{
        0x00000088u, 0x000000E8u, 0x00000260u, 0x00000310u};
    constexpr std::array<std::uint32_t, 4> kExpectedLo{
        0x00000114u, 0x0000023Cu, 0x0000042Cu, 0x000005A4u};

    Ps2GeneratedMmiProbeResult result{};
    R5900Context context{};
    R5900Context *ctx = &context;
    SET_GPR_U32(ctx, 31u, kReturnPc);
    Ps2MmiWriteGprVector(ctx, 1u, kInitialHi);
    Ps2MmiWriteGprVector(ctx, 2u, kInitialLo);
    Ps2MmiWriteGprVector(ctx, 5u, kLhs);
    Ps2MmiWriteGprVector(ctx, 6u, kRhs);

    MKSM_RECOMP_GENERATED_MMI_ENTRY(rdram, ctx, nullptr);

    result.pc = ctx->pc;
    result.initialHi = Ps2MmiReadGprVector(ctx, 3u).words;
    result.initialLo = Ps2MmiReadGprVector(ctx, 4u).words;
    result.destination = Ps2MmiReadGprVector(ctx, 7u).words;
    result.hi = Ps2MmiReadGprVector(ctx, 8u).words;
    result.lo = Ps2MmiReadGprVector(ctx, 9u).words;

    if (result.pc != kReturnPc || ctx->in_delay_slot)
        result.failures |= 1u << 0u;
    if (result.initialHi != kInitialHi.words || result.initialLo != kInitialLo.words)
        result.failures |= 1u << 1u;
    if (result.destination != kExpectedDestination)
        result.failures |= 1u << 2u;
    if (result.hi != kExpectedHi || result.lo != kExpectedLo)
        result.failures |= 1u << 3u;

    const auto state = Ps2MmiReadHiLo(ctx);
    if (state.hi.words != kExpectedHi || state.lo.words != kExpectedLo)
        result.failures |= 1u << 4u;
    if (GPR_U32(ctx, 0u) != 0u)
        result.failures |= 1u << 5u;

    return result;
}
