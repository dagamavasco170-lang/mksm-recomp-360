#include "ps2_generated_runtime_abi.h"

#include "xsdk/log.h"

#include <array>
#include <cstdint>

bool checkGeneratedRecompProbe()
{
    constexpr std::uint32_t kReturnPc = 0xCAFE1234u;
    R5900Context context{};
    std::array<std::uint8_t, 16> unusedGuestMemory{};
    PS2Runtime *runtime = nullptr;

    SET_GPR_U32(&context, 31u, kReturnPc);
    mksm_ee_codegen_probe_0x100000(unusedGuestMemory.data(), &context, runtime);

    std::uint32_t failures = 0u;
    const std::uint32_t r1 = GPR_U32(&context, 1u);
    const std::uint32_t r2 = GPR_U32(&context, 2u);
    const std::uint32_t r3 = GPR_U32(&context, 3u);
    const std::uint32_t r4 = GPR_U32(&context, 4u);
    const std::uint32_t r5 = GPR_U32(&context, 5u);
    const std::uint32_t r6 = GPR_U32(&context, 6u);
    const std::uint32_t r7 = GPR_U32(&context, 7u);

    if (r1 != 0x12345678u || r2 != 0x12345677u || r3 != 0x12345688u)
        failures |= 1u << 0u;
    if (r4 != 0x12345688u || r5 != 0x2468AD10u)
        failures |= 1u << 1u;
    if (r6 != 9u || r7 != 12u)
        failures |= 1u << 2u;
    if (context.pc != kReturnPc || context.in_delay_slot)
        failures |= 1u << 3u;
    if (GPR_U32(&context, 0u) != 0u || context.r[0].low64 != 0u)
        failures |= 1u << 4u;

    xsdk_log_write("MKSM_RECOMP_XENON_TRACE failures=%08X pc=%08X r1=%08X r2=%08X "
                   "r3=%08X r4=%08X r5=%08X r6=%08X r7=%08X",
                   static_cast<unsigned>(failures),
                   static_cast<unsigned>(context.pc),
                   static_cast<unsigned>(r1), static_cast<unsigned>(r2),
                   static_cast<unsigned>(r3), static_cast<unsigned>(r4),
                   static_cast<unsigned>(r5), static_cast<unsigned>(r6),
                   static_cast<unsigned>(r7));
    return failures == 0u;
}
