#include "ps2_ee_dispatcher.h"

#include <array>
#include <cassert>
#include <cstdint>
#include <span>

using namespace ps2::xbox360::ee;

int main()
{
    std::array<std::uint8_t, 256> ram{};
    CpuState cpu{};
    cpu.pc = 0x40;
    const auto mem = std::span<std::uint8_t>(ram);
    // LUI r1,0x1234; ORI r1,r1,0x5678; ADDIU r2,r1,-1; XORI r3,r2,0xFFFF
    const std::array<std::uint32_t,4> code{
        (0x0Fu<<26)|(1u<<16)|0x1234u,
        (0x0Du<<26)|(1u<<21)|(1u<<16)|0x5678u,
        (0x09u<<26)|(1u<<21)|(2u<<16)|0xFFFFu,
        (0x0Eu<<26)|(2u<<21)|(3u<<16)|0xFFFFu
    };
    for (std::size_t i=0;i<code.size();++i)
        assert(guest_memory::writeLittleEndian(mem,cpu.pc+static_cast<std::uint32_t>(i*4),code[i]));
    for (unsigned i=0;i<code.size();++i)
        assert(step(cpu,mem).status==StepStatus::Executed);
    assert(simd::readLane<std::uint32_t>(cpu.readGpr(1),0)==0x12345678u);
    assert(simd::readLane<std::uint32_t>(cpu.readGpr(2),0)==0x12345677u);
    assert(simd::readLane<std::uint32_t>(cpu.readGpr(3),0)==0xEDCBA988u);
    assert(cpu.pc==0x50u && cpu.retiredInstructions==4u);
}
