#include "ps2_ee_dispatcher.h"
#include <array>
#include <cassert>
#include <cstdint>
using namespace ps2::xbox360::ee;
int main()
{
    std::array<std::uint8_t, 256> ram{};
    CpuState cpu{}; cpu.pc = 0x20;
    const std::uint32_t addu = (1u<<21)|(2u<<16)|(3u<<11)|0x21u;
    assert(ps2::xbox360::guest_memory::writeLittleEndian(std::span<std::uint8_t>(ram),cpu.pc,addu));
    ps2::xbox360::simd::Vector128 one{},two{};
    ps2::xbox360::simd::writeLane<std::uint32_t>(one,0,7u);
    ps2::xbox360::simd::writeLane<std::uint32_t>(two,0,9u);
    cpu.writeGpr(1,one); cpu.writeGpr(2,two);
    auto result=step(cpu,ram);
    assert(result.status==StepStatus::Executed);
    assert(ps2::xbox360::simd::readLane<std::uint32_t>(cpu.readGpr(3),0)==16u);
    assert(cpu.pc==0x24u&&cpu.retiredInstructions==1u);

    // Taken BEQ executes the delay instruction, then skips the next word.
    const std::uint32_t beq=(4u<<26)|(1u<<21)|(2u<<16)|2u;
    const std::uint32_t delay=(9u<<26)|(3u<<21)|(4u<<16)|5u;
    const std::uint32_t skipped=(9u<<26)|(3u<<21)|(5u<<16)|99u;
    const std::uint32_t target=(9u<<26)|(3u<<21)|(6u<<16)|7u;
    assert(ps2::xbox360::guest_memory::writeLittleEndian(std::span<std::uint8_t>(ram),0x24u,beq));
    assert(ps2::xbox360::guest_memory::writeLittleEndian(std::span<std::uint8_t>(ram),0x28u,delay));
    assert(ps2::xbox360::guest_memory::writeLittleEndian(std::span<std::uint8_t>(ram),0x2Cu,skipped));
    assert(ps2::xbox360::guest_memory::writeLittleEndian(std::span<std::uint8_t>(ram),0x30u,target));
    cpu.writeGpr(1,one); cpu.writeGpr(2,one); cpu.pc=0x24u;
    result=step(cpu,ram); assert(result.status==StepStatus::Executed&&cpu.pc==0x28u&&cpu.branchDelayPending);
    result=step(cpu,ram); assert(result.status==StepStatus::Executed&&cpu.pc==0x30u&&!cpu.branchDelayPending);
    assert(ps2::xbox360::simd::readLane<std::uint32_t>(cpu.readGpr(4),0)==12u);
    result=step(cpu,ram); assert(result.status==StepStatus::Executed&&cpu.pc==0x34u);
    assert(ps2::xbox360::simd::readLane<std::uint32_t>(cpu.readGpr(5),0)==0u);
    assert(ps2::xbox360::simd::readLane<std::uint32_t>(cpu.readGpr(6),0)==7u);

    // JR delays its transfer and JALR writes the return address (PC+8).
    const std::uint32_t jr=(7u<<21)|8u;
    const std::uint32_t jrDelay=(9u<<26)|(3u<<21)|(8u<<16)|1u;
    const std::uint32_t jrTarget=(9u<<26)|(3u<<21)|(9u<<16)|9u;
    assert(ps2::xbox360::guest_memory::writeLittleEndian(std::span<std::uint8_t>(ram),0x34u,jr));
    assert(ps2::xbox360::guest_memory::writeLittleEndian(std::span<std::uint8_t>(ram),0x38u,jrDelay));
    assert(ps2::xbox360::guest_memory::writeLittleEndian(std::span<std::uint8_t>(ram),0x50u,jrTarget));
    ps2::xbox360::simd::writeLane<std::uint32_t>(one,0,0x50u); cpu.writeGpr(7,one);
    result=step(cpu,ram); assert(result.status==StepStatus::Executed&&cpu.pc==0x38u&&cpu.branchDelayPending);
    result=step(cpu,ram); assert(result.status==StepStatus::Executed&&cpu.pc==0x50u&&!cpu.branchDelayPending);
    result=step(cpu,ram); assert(result.status==StepStatus::Executed);
    assert(ps2::xbox360::simd::readLane<std::uint32_t>(cpu.readGpr(9),0)==9u);

    // Unsupported instruction and invalid fetch leave architectural state unchanged.
    const std::uint32_t cop1=0x44000000u;
    assert(ps2::xbox360::guest_memory::writeLittleEndian(std::span<std::uint8_t>(ram),cpu.pc,cop1));
    const auto beforePc=cpu.pc; const auto beforeRetired=cpu.retiredInstructions;
    result=step(cpu,ram); assert(result.status==StepStatus::Unsupported);
    assert(cpu.pc==beforePc&&cpu.retiredInstructions==beforeRetired);
    std::span<std::uint8_t> empty{};
    result=step(cpu,empty); assert(result.status==StepStatus::InvalidMemory);
    assert(cpu.pc==beforePc&&cpu.retiredInstructions==beforeRetired);
}
