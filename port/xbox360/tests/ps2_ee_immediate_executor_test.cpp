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
        assert(ps2::xbox360::guest_memory::writeLittleEndian(mem,cpu.pc+static_cast<std::uint32_t>(i*4),code[i]));
    for (unsigned i=0;i<code.size();++i)
        assert(step(cpu,mem).status==StepStatus::Executed);
    assert(ps2::xbox360::simd::readLane<std::uint32_t>(cpu.readGpr(1),0)==0x12345678u);
    assert(ps2::xbox360::simd::readLane<std::uint32_t>(cpu.readGpr(2),0)==0x12345677u);
    assert(ps2::xbox360::simd::readLane<std::uint32_t>(cpu.readGpr(3),0)==0x1234A988u);
    assert(cpu.pc==0x50u && cpu.retiredInstructions==4u);

    // SLTI/SLTIU use a sign-extended 16-bit immediate; SLTI is signed,
    // while SLTIU treats the sign-extended immediate as an unsigned value.
    ps2::xbox360::simd::Vector128 negative{};
    ps2::xbox360::simd::writeLane<std::uint32_t>(negative,0,0xFFFFFFFFu);
    cpu.writeGpr(12,negative);
    const std::array<std::uint32_t,6> compareCode{
        (0x0Au<<26)|(12u<<21)|(4u<<16)|0xFFFFu, // SLTI r4,r12,-1 -> 0
        (0x0Au<<26)|(12u<<21)|(5u<<16)|1u,     // SLTI r5,r12,1 -> 1
        (0x0Bu<<26)|(12u<<21)|(6u<<16)|1u,     // SLTIU r6,r12,1 -> 0
        (0x0Bu<<26)|(0u<<21)|(7u<<16)|0xFFFFu, // SLTIU r7,r0,-1 -> 1
        (0x0Au<<26)|(0u<<21)|(8u<<16)|0u,     // SLTI r8,r0,0 -> 0
        (0x0Bu<<26)|(12u<<21)|(0u<<16)|0xFFFFu // SLTIU r0,r12,-1 (discard)
    };
    for (std::size_t i=0;i<compareCode.size();++i)
        assert(ps2::xbox360::guest_memory::writeLittleEndian(
            mem,cpu.pc+static_cast<std::uint32_t>(i*4),compareCode[i]));
    for (std::size_t i=0;i<compareCode.size();++i)
        assert(step(cpu,mem).status==StepStatus::Executed);
    assert(ps2::xbox360::simd::readLane<std::uint32_t>(cpu.readGpr(4),0)==0u);
    assert(ps2::xbox360::simd::readLane<std::uint32_t>(cpu.readGpr(5),0)==1u);
    assert(ps2::xbox360::simd::readLane<std::uint32_t>(cpu.readGpr(6),0)==0u);
    assert(ps2::xbox360::simd::readLane<std::uint32_t>(cpu.readGpr(7),0)==1u);
    assert(ps2::xbox360::simd::readLane<std::uint32_t>(cpu.readGpr(8),0)==0u);
    assert(ps2::xbox360::simd::readLane<std::uint32_t>(cpu.readGpr(0),0)==0u);
    assert(cpu.pc==0x68u && cpu.retiredInstructions==10u);
}
