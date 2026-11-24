#pragma once

#include "ps2_generated_runtime_abi.h"

#include <cstdint>
#include <vector>

struct Ps2GeneratedMemoryProbeResult
{
    std::uint32_t failures{};
    std::uint32_t pc{};
    std::uint32_t r1{};
    std::uint32_t r2{};
    std::uint32_t r3{};
    std::uint32_t r4{};
    std::uint32_t r5{};
    std::uint32_t r6{};
    std::uint32_t r7{};
    std::uint32_t loadWord{};
    std::uint32_t loadByte{};
    std::uint64_t loadSignedByte{};
    std::uint32_t loadHalf{};
    std::uint64_t loadDouble{};
    std::uint32_t loadByteAfterStore{};
    std::uint32_t loadHalfAfterStore{};
};

// Shared by the host differential runner and the Xenon smoke XEX so both
// execute the exact same PS2Recomp-generated function and state assertions.
[[nodiscard]] inline Ps2GeneratedMemoryProbeResult runGeneratedMemoryProbe()
{
    constexpr std::uint32_t kReturnPc = 0xCAFE1234u;
    std::vector<std::uint8_t> rdram;
    Ps2GeneratedMemoryProbeResult result{};

    try
    {
        rdram.resize(ps2::xbox360::generated_abi::kGuestRdramSize);
    }
    catch (...)
    {
        result.failures = 1u << 31u;
        return result;
    }

    R5900Context context{};
    PS2Runtime *runtime = nullptr;
    SET_GPR_U32(&context, 31u, kReturnPc);
    SET_GPR_U32(&context, 8u, 0x01FFFFF8u); // Last aligned RDRAM word; later ops verify mirrored aliases.
    SET_GPR_U32(&context, 9u, 0xA1B2C3D4u);
    SET_GPR_U64(&context, 14u, 0x0123456789ABCDEFull);
    SET_GPR_U32(&context, 16u, 0x5Au);
    SET_GPR_U32(&context, 18u, 0xBEEFu);

    mksm_ee_codegen_probe_0x100000(rdram.data(), &context, runtime);

    result.pc = context.pc;
    result.r1 = GPR_U32(&context, 1u);
    result.r2 = GPR_U32(&context, 2u);
    result.r3 = GPR_U32(&context, 3u);
    result.r4 = GPR_U32(&context, 4u);
    result.r5 = GPR_U32(&context, 5u);
    result.r6 = GPR_U32(&context, 6u);
    result.r7 = GPR_U32(&context, 7u);
    result.loadWord = GPR_U32(&context, 10u);
    result.loadByte = GPR_U32(&context, 11u);
    result.loadSignedByte = GPR_U64(&context, 12u);
    result.loadHalf = GPR_U32(&context, 13u);
    result.loadDouble = GPR_U64(&context, 15u);
    result.loadByteAfterStore = GPR_U32(&context, 17u);
    result.loadHalfAfterStore = GPR_U32(&context, 19u);

    if (result.r1 != 0x12345678u || result.r2 != 0x12345677u ||
        result.r3 != 0x12345688u)
        result.failures |= 1u << 0u;
    if (result.r4 != 0x12345688u || result.r5 != 0x2468AD10u)
        result.failures |= 1u << 1u;
    if (result.r6 != 9u || result.r7 != 12u)
        result.failures |= 1u << 2u;
    if (result.pc != kReturnPc || context.in_delay_slot)
        result.failures |= 1u << 3u;
    if (result.loadWord != 0xA1B2C3D4u)
        result.failures |= 1u << 4u;
    if (result.loadByte != 0xC3u ||
        result.loadSignedByte != 0xFFFFFFFFFFFFFFA1ull ||
        result.loadHalf != 0xA1B2u)
        result.failures |= 1u << 5u;
    if (result.loadDouble != 0x0123456789ABCDEFull ||
        result.loadByteAfterStore != 0x5Au ||
        result.loadHalfAfterStore != 0xBEEFu)
        result.failures |= 1u << 6u;

    constexpr std::uint32_t kRdramMask =
        static_cast<std::uint32_t>(ps2::xbox360::generated_abi::kGuestRdramSize - 1u);
    if (rdram[kRdramMask - 7u] != 0xD4u ||
        rdram[kRdramMask - 6u] != 0xC3u ||
        rdram[kRdramMask - 5u] != 0xB2u ||
        rdram[kRdramMask - 4u] != 0xA1u ||
        rdram[0] != 0xEFu || rdram[1] != 0xCDu ||
        rdram[2] != 0xABu || rdram[3] != 0x89u ||
        rdram[4] != 0x67u || rdram[5] != 0x45u ||
        rdram[6] != 0x23u || rdram[7] != 0x5Au ||
        rdram[8] != 0xEFu || rdram[9] != 0xBEu)
        result.failures |= 1u << 7u;
    if (GPR_U32(&context, 0u) != 0u || context.r[0].low64 != 0u)
        result.failures |= 1u << 8u;

    return result;
}
