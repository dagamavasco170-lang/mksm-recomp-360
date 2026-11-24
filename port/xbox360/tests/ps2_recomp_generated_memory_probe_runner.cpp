#include "ps2_recomp_generated_memory_probe.h"

#include <cstdio>

int main()
{
    const auto result = runGeneratedMemoryProbe();
    std::printf("MKSM_RECOMP_MEMORY_TRACE failures=%08X pc=%08X r1=%08X r2=%08X "
                "r3=%08X r4=%08X r5=%08X r6=%08X r7=%08X "
                "lw=%08X lbu=%08X lb=%016llX lhu=%04X ld=%016llX "
                "lbu_after=%02X lhu_after=%04X spw=%08X spb=%02X "
                "sph=%04X spd=%016llX aliasw=%08X aliasb=%02X isolated=%08X "
                "mmi=%08X dest=%08X/%08X/%08X/%08X\n",
                static_cast<unsigned>(result.failures),
                static_cast<unsigned>(result.pc),
                static_cast<unsigned>(result.r1), static_cast<unsigned>(result.r2),
                static_cast<unsigned>(result.r3), static_cast<unsigned>(result.r4),
                static_cast<unsigned>(result.r5), static_cast<unsigned>(result.r6),
                static_cast<unsigned>(result.r7),
                static_cast<unsigned>(result.loadWord),
                static_cast<unsigned>(result.loadByte),
                static_cast<unsigned long long>(result.loadSignedByte),
                static_cast<unsigned>(result.loadHalf),
                static_cast<unsigned long long>(result.loadDouble),
                static_cast<unsigned>(result.loadByteAfterStore),
                static_cast<unsigned>(result.loadHalfAfterStore),
                static_cast<unsigned>(result.scratchpadWord),
                static_cast<unsigned>(result.scratchpadByte),
                static_cast<unsigned>(result.scratchpadHalf),
                static_cast<unsigned long long>(result.scratchpadDouble),
                static_cast<unsigned>(result.scratchpadAliasWord),
                static_cast<unsigned>(result.scratchpadAliasByte),
                static_cast<unsigned>(result.scratchpadIsolationWord),
                static_cast<unsigned>(result.mmi.failures),
                static_cast<unsigned>(result.mmi.destination[0]),
                static_cast<unsigned>(result.mmi.destination[1]),
                static_cast<unsigned>(result.mmi.destination[2]),
                static_cast<unsigned>(result.mmi.destination[3]));
    return result.failures == 0u ? 0 : 1;
}
