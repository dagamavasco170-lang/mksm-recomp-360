#pragma once

#include "ps2_generated_runtime_abi.h"
#include "mksm_generated_probe_entry.h"
#include "ps2_recomp_generated_mmi_probe.h"

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
    std::uint32_t scratchpadWord{};
    std::uint32_t scratchpadByte{};
    std::uint32_t scratchpadHalf{};
    std::uint64_t scratchpadDouble{};
    std::uint32_t scratchpadAliasWord{};
    std::uint32_t scratchpadAliasByte{};
    std::uint32_t scratchpadIsolationWord{};
    std::uint32_t sprDmaMadrFrom{}, sprDmaQwcFrom{}, sprDmaSadrFrom{}, sprDmaChcrFrom{};
    std::uint32_t sprDmaStadr{}, sprDmaDstat{}, sprDmaRdramWord{};
    std::uint32_t sprDmaMadrTo{}, sprDmaQwcTo{}, sprDmaSadrTo{}, sprDmaChcrTo{};
    std::uint32_t sprDmaScratchWord0{}, sprDmaScratchWord1{};
    Ps2GeneratedMmiProbeResult mmi{};
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
    PS2Runtime runtimeStorage{};
    PS2Runtime *runtime = &runtimeStorage;
    SET_GPR_U32(&context, 31u, kReturnPc);
    SET_GPR_U32(&context, 8u, 0x01FFFFF8u); // Last aligned word in the default RDRAM window.
    SET_GPR_U32(&context, 20u, 0x20000000u); // Base of the documented uncached 32 MiB RDRAM alias.
    SET_GPR_U32(&context, 9u, 0xA1B2C3D4u);
    SET_GPR_U64(&context, 14u, 0x0123456789ABCDEFull);
    SET_GPR_U32(&context, 16u, 0x5Au);
    SET_GPR_U32(&context, 18u, 0xBEEFu);

    MKSM_RECOMP_GENERATED_PROBE_ENTRY(rdram.data(), &context, runtime);

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

    // Execute actual PS2Recomp-generated scalar load/store instructions against
    // the canonical scratchpad window and the desktop runtime's bit-31 alias.
    R5900Context scratchContext{};
    SET_GPR_U32(&scratchContext, 31u, kReturnPc);
    SET_GPR_U32(&scratchContext, 8u, 0x70000000u);
    SET_GPR_U32(&scratchContext, 9u, 0xA1B2C3D4u);
    SET_GPR_U32(&scratchContext, 11u, 0x5Au);
    SET_GPR_U32(&scratchContext, 13u, 0xBEEFu);
    SET_GPR_U64(&scratchContext, 15u, 0x0123456789ABCDEFull);
    SET_GPR_U32(&scratchContext, 17u, 0xF0000000u);
    SET_GPR_U32(&scratchContext, 19u, 0xC7u);
    SET_GPR_U32(&scratchContext, 21u, 0xDEADBEEFu);

    MKSM_RECOMP_GENERATED_SCRATCHPAD_ENTRY(
        rdram.data(), &scratchContext, runtime);

    result.scratchpadWord = GPR_U32(&scratchContext, 10u);
    result.scratchpadByte = GPR_U32(&scratchContext, 12u);
    result.scratchpadHalf = GPR_U32(&scratchContext, 14u);
    result.scratchpadDouble = GPR_U64(&scratchContext, 16u);
    result.scratchpadAliasWord = GPR_U32(&scratchContext, 18u);
    result.scratchpadAliasByte = GPR_U32(&scratchContext, 20u);
    result.scratchpadIsolationWord = GPR_U32(&scratchContext, 22u);

    if (scratchContext.pc != kReturnPc || scratchContext.in_delay_slot)
        result.failures |= 1u << 16u;
    if (result.scratchpadWord != 0xA1B2C3D4u ||
        result.scratchpadByte != 0x5Au ||
        result.scratchpadHalf != 0xBEEFu)
        result.failures |= 1u << 17u;
    if (result.scratchpadDouble != 0x0123456789ABCDEFull ||
        result.scratchpadAliasWord != 0xA1B2C3D4u ||
        result.scratchpadAliasByte != 0xC7u)
        result.failures |= 1u << 18u;
    if (result.scratchpadIsolationWord != 0xDEADBEEFu)
        result.failures |= 1u << 19u;

    const auto scratchpad = runtime->eeScratchpad();
    if (scratchpad[0] != 0xD4u || scratchpad[1] != 0xC3u ||
        scratchpad[2] != 0xB2u || scratchpad[3] != 0xA1u ||
        scratchpad[4] != 0x5Au || scratchpad[6] != 0xEFu ||
        scratchpad[7] != 0xBEu || scratchpad[8] != 0xEFu ||
        scratchpad[9] != 0xC7u || scratchpad[10] != 0xABu ||
        scratchpad[11] != 0x89u || scratchpad[12] != 0x67u ||
        scratchpad[13] != 0x45u || scratchpad[14] != 0x23u ||
        scratchpad[15] != 0x01u || scratchpad[16] != 0xEFu ||
        scratchpad[17] != 0xBEu || scratchpad[18] != 0xADu ||
        scratchpad[19] != 0xDEu || rdram[0x10u] != 0u ||
        rdram[0x11u] != 0u || rdram[0x12u] != 0u || rdram[0x13u] != 0u)
        result.failures |= 1u << 20u;

    // Real translated SW/LW instructions drive both normal SPR DMA
    // directions through the per-runtime EE MMIO bank.
    constexpr std::uint32_t kSprStart=0x3FF0u;
    for(std::uint32_t i=0;i<32u;++i){
        runtime->eeScratchpad()[(kSprStart+i)&0x3FFFu]=static_cast<std::uint8_t>(0x40u+i);
        rdram[0x00090000u+i]=static_cast<std::uint8_t>(0xA0u+i);
    }
    R5900Context dmaContext{};
    SET_GPR_U32(&dmaContext,31u,kReturnPc);
    SET_GPR_U32(&dmaContext,30u,0x70000000u);
    MKSM_RECOMP_GENERATED_SPR_DMA_ENTRY(rdram.data(),&dmaContext,runtime);

    result.sprDmaMadrFrom=GPR_U32(&dmaContext,11u);
    result.sprDmaQwcFrom=GPR_U32(&dmaContext,12u);
    result.sprDmaSadrFrom=GPR_U32(&dmaContext,13u);
    result.sprDmaChcrFrom=GPR_U32(&dmaContext,14u);
    result.sprDmaStadr=GPR_U32(&dmaContext,16u);
    result.sprDmaDstat=GPR_U32(&dmaContext,18u);
    result.sprDmaRdramWord=GPR_U32(&dmaContext,20u);
    result.sprDmaMadrTo=GPR_U32(&dmaContext,6u);
    result.sprDmaQwcTo=GPR_U32(&dmaContext,24u);
    result.sprDmaSadrTo=GPR_U32(&dmaContext,26u);
    result.sprDmaChcrTo=GPR_U32(&dmaContext,2u);
    result.sprDmaScratchWord0=GPR_U32(&dmaContext,29u);
    result.sprDmaScratchWord1=GPR_U32(&dmaContext,30u);

    if(dmaContext.pc!=kReturnPc||dmaContext.in_delay_slot)
        result.failures|=1u<<24u;
    if(result.sprDmaMadrFrom!=0x20080020u||result.sprDmaQwcFrom!=0u||
       result.sprDmaSadrFrom!=0x10u||result.sprDmaChcrFrom!=0u||
       result.sprDmaStadr!=0x20080020u)
        result.failures|=1u<<25u;
    if(result.sprDmaRdramWord!=0x43424140u)
        result.failures|=1u<<26u;
    if(result.sprDmaMadrTo!=0x00090020u||result.sprDmaQwcTo!=0u||
       result.sprDmaSadrTo!=0x10u||result.sprDmaChcrTo!=0u)
        result.failures|=1u<<27u;
    if((result.sprDmaDstat&0x300u)!=0x300u||
       result.sprDmaScratchWord0!=0xA3A2A1A0u||
       result.sprDmaScratchWord1!=0xA7A6A5A4u)
        result.failures|=1u<<28u;

    result.mmi = runGeneratedMmiProbe(rdram.data());
    if (result.mmi.failures != 0u)
        result.failures |= 1u << 21u;

    return result;
}
