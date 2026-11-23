#include "ps2_ee_load_store_executor.h"

#include <array>
#include <cassert>
#include <cstdint>

using namespace ps2::xbox360;
using namespace ps2::xbox360::ee;

static DecodedInstruction ins(std::uint8_t opcode, std::uint8_t base,
                              std::uint8_t rt, std::uint16_t imm)
{
    DecodedInstruction d{};
    d.kind = InstructionKind::LoadStore;
    d.opcode = opcode;
    d.rs = base;
    d.rt = rt;
    d.immediate = imm;
    return d;
}

int main()
{
    std::array<std::uint8_t, 256> ram{};
    CpuState cpu{};
    simd::Vector128 base{}, source{};
    simd::writeLane<std::uint32_t>(base, 0, 32);
    simd::writeLane<std::uint32_t>(source, 0, 0x89ABCDEFu);
    cpu.writeGpr(1, base);
    cpu.writeGpr(2, source);

    assert(executeLoadStore(cpu, ins(0x2B, 1, 2, 0), ram) == LoadStoreStatus::Executed);
    assert(ram[32] == 0xEF && ram[33] == 0xCD && ram[34] == 0xAB && ram[35] == 0x89);
    assert(cpu.pc == 4 && cpu.retiredInstructions == 1);

    assert(executeLoadStore(cpu, ins(0x23, 1, 3, 0), ram) == LoadStoreStatus::Executed);
    assert(simd::readLane<std::uint32_t>(cpu.readGpr(3), 0) == 0x89ABCDEFu);

    ram[40] = 0x80;
    assert(executeLoadStore(cpu, ins(0x20, 1, 4, 8), ram) == LoadStoreStatus::Executed);
    assert(simd::readLane<std::uint32_t>(cpu.readGpr(4), 0) == 0xFFFFFF80u);
    assert(executeLoadStore(cpu, ins(0x24, 1, 5, 8), ram) == LoadStoreStatus::Executed);
    assert(simd::readLane<std::uint32_t>(cpu.readGpr(5), 0) == 0x80u);

    const auto oldPc = cpu.pc;
    const auto oldRetired = cpu.retiredInstructions;
    assert(executeLoadStore(cpu, ins(0x23, 1, 6, 1), ram) == LoadStoreStatus::AddressError);
    assert(cpu.pc == oldPc && cpu.retiredInstructions == oldRetired);
    assert(executeLoadStore(cpu, ins(0x30, 1, 6, 0), ram) == LoadStoreStatus::Unsupported);
    assert(cpu.pc == oldPc && cpu.retiredInstructions == oldRetired);
}
