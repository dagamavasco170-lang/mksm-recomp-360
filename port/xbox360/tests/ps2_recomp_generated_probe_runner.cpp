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
    SET_GPR_U32(&context, 31, returnPc);

    mksm_ee_codegen_probe_0x100000(rdram.data(), &context, nullptr);

    std::uint32_t failures = 0u;
    const std::uint32_t r1 = GPR_U32(&context, 1);
    const std::uint32_t r2 = GPR_U32(&context, 2);
    const std::uint32_t r3 = GPR_U32(&context, 3);
    const std::uint32_t r4 = GPR_U32(&context, 4);
    const std::uint32_t r5 = GPR_U32(&context, 5);
    const std::uint32_t r6 = GPR_U32(&context, 6);
    const std::uint32_t r7 = GPR_U32(&context, 7);

    if (r1 != 0x12345678u || r2 != 0x12345677u || r3 != 0x12345688u)
        failures |= 1u << 0u;
    if (r4 != 0x12345688u || r5 != 0x2468AD10u)
        failures |= 1u << 1u;
    if (r6 != 9u || r7 != 12u)
        failures |= 1u << 2u;
    if (context.pc != returnPc || context.in_delay_slot)
        failures |= 1u << 3u;

    std::printf("MKSM_RECOMP_TRACE failures=%08X pc=%08X r1=%08X r2=%08X "
                "r3=%08X r4=%08X r5=%08X r6=%08X r7=%08X\n",
                static_cast<unsigned>(failures),
                static_cast<unsigned>(context.pc),
                static_cast<unsigned>(r1), static_cast<unsigned>(r2),
                static_cast<unsigned>(r3), static_cast<unsigned>(r4),
                static_cast<unsigned>(r5), static_cast<unsigned>(r6),
                static_cast<unsigned>(r7));
    return failures == 0u ? 0 : 1;
}
