#include "ps2_ee_cpu_state.h"

#include <cassert>
#include <cstdint>

using namespace ps2::xbox360::simd;
using namespace ps2::xbox360::ee;

int main()
{
    CpuState cpu{};
    Vector128 source{};
    for (std::size_t i = 0; i < source.bytes.size(); ++i)
        source.bytes[i] = static_cast<std::uint8_t>(i * 13u + 5u);

    cpu.writeGpr(0, source);
    assert(cpu.readGpr(0).bytes == Vector128{}.bytes); // architectural zero register
    cpu.writeGpr(7, source);
    assert(cpu.readGpr(7).bytes == source.bytes);

    cpu.pmthi(7);
    cpu.pmtlo(7);
    cpu.writeGpr(8, Vector128{});
    cpu.pmfhi(8);
    assert(cpu.readGpr(8).bytes == source.bytes);
    cpu.pmflo(9);
    assert(cpu.readGpr(9).bytes == source.bytes);

    cpu.pc = 0x00100000u;
    cpu.advancePc();
    assert(cpu.pc == 0x00100004u);
    cpu.advancePc(8u);
    assert(cpu.pc == 0x0010000Cu);

    cpu.recordRetiredInstruction();
    cpu.recordRetiredInstruction();
    assert(cpu.retiredInstructions == 2u);
    return 0;
}
