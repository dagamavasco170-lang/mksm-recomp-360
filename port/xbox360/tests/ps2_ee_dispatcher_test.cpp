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
    assert(ps2::xbox360::simd::readLane<std::uint32_t>(cpu.readGpr(4),0)==21u);
    result=step(cpu,ram); assert(result.status==StepStatus::Executed&&cpu.pc==0x34u);
    assert(ps2::xbox360::simd::readLane<std::uint32_t>(cpu.readGpr(5),0)==0u);
    assert(ps2::xbox360::simd::readLane<std::uint32_t>(cpu.readGpr(6),0)==23u);

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
    assert(ps2::xbox360::simd::readLane<std::uint32_t>(cpu.readGpr(9),0)==25u);

    // Not-taken branch still executes its delay slot and continues sequentially.
    const beqNotTaken=(4u<<26)|(1u<<21)|(2u<<16)|2u;
    const notTakenDelay=(9u<<26)|(3u<<21)|(10u<<16)|3u;
    const notTakenNext=(9u<<26)|(3u<<21)|(11u<<16)|4u;
    assert(ps2::xbox360::guest_memory::writeLittleEndian(std::span<std::uint8_t>(ram),0x60u,beqNotTaken));
    assert(ps2::xbox360::guest_memory::writeLittleEndian(std::span<std::uint8_t>(ram),0x64u,notTakenDelay));
    assert(ps2::xbox360::guest_memory::writeLittleEndian(std::span<std::uint8_t>(ram),0x68u,notTakenNext));
    cpu.pc=0x60u; cpu.writeGpr(1,one); cpu.writeGpr(2,two);
    result=step(cpu,ram); assert(result.status==StepStatus::Executed&&cpu.pc==0x64u&&cpu.branchDelayPending);
    result=step(cpu,ram); assert(result.status==StepStatus::Executed&&cpu.pc==0x68u&&!cpu.branchDelayPending);
    assert(ps2::xbox360::simd::readLane<std::uint32_t>(cpu.readGpr(10),0)==26u);
    result=step(cpu,ram); assert(result.status==StepStatus::Executed);
    assert(ps2::xbox360::simd::readLane<std::uint32_t>(cpu.readGpr(11),0)==27u);

    // BLTZ with a negative register takes the relative target after its delay slot.
    const std::uint32_t bltz=(1u<<26)|(12u<<21)|1u;
    const std::uint32_t bltzDelay=(9u<<26)|(3u<<21)|(12u<<16)|5u;
    const std::uint32_t bltzTarget=(9u<<26)|(3u<<21)|(13u<<16)|6u;
    assert(ps2::xbox360::guest_memory::writeLittleEndian(std::span<std::uint8_t>(ram),0x70u,bltz));
    assert(ps2::xbox360::guest_memory::writeLittleEndian(std::span<std::uint8_t>(ram),0x74u,bltzDelay));
    assert(ps2::xbox360::guest_memory::writeLittleEndian(std::span<std::uint8_t>(ram),0x78u,bltzTarget));
    ps2::xbox360::simd::writeLane<std::uint32_t>(one,0,0xFFFFFFFFu); cpu.writeGpr(12,one); cpu.pc=0x70u;
    result=step(cpu,ram); assert(result.status==StepStatus::Executed&&cpu.pc==0x74u);
    result=step(cpu,ram); assert(result.status==StepStatus::Executed&&cpu.pc==0x78u);
    result=step(cpu,ram); assert(result.status==StepStatus::Executed);
    assert(ps2::xbox360::simd::readLane<std::uint32_t>(cpu.readGpr(13),0)==29u);

    // JAL writes PC+8 and runs the delay slot before jumping.
    const std::uint32_t jal=(3u<<26)|0x28u;
    const std::uint32_t jalDelay=(9u<<26)|(3u<<21)|(14u<<16)|7u;
    const std::uint32_t jalTarget=(9u<<26)|(3u<<21)|(15u<<16)|8u;
    assert(ps2::xbox360::guest_memory::writeLittleEndian(std::span<std::uint8_t>(ram),0x80u,jal));
    assert(ps2::xbox360::guest_memory::writeLittleEndian(std::span<std::uint8_t>(ram),0x84u,jalDelay));
    assert(ps2::xbox360::guest_memory::writeLittleEndian(std::span<std::uint8_t>(ram),0xA0u,jalTarget));
    cpu.pc=0x80u;
    result=step(cpu,ram); assert(result.status==StepStatus::Executed&&cpu.pc==0x84u);
    assert(ps2::xbox360::simd::readLane<std::uint32_t>(cpu.readGpr(31),0)==0x88u);
    result=step(cpu,ram); assert(result.status==StepStatus::Executed&&cpu.pc==0xA0u);
    result=step(cpu,ram); assert(result.status==StepStatus::Executed);
    assert(ps2::xbox360::simd::readLane<std::uint32_t>(cpu.readGpr(15),0)==31u);

    // JALR writes its selected link register, not only register 31.
    const std::uint32_t jalr=(7u<<21)|(10u<<11)|9u;
    const std::uint32_t jalrDelay=(9u<<26)|(3u<<21)|(16u<<16)|10u;
    const std::uint32_t jalrTarget=(9u<<26)|(3u<<21)|(17u<<16)|11u;
    assert(ps2::xbox360::guest_memory::writeLittleEndian(std::span<std::uint8_t>(ram),0xB0u,jalr));
    assert(ps2::xbox360::guest_memory::writeLittleEndian(std::span<std::uint8_t>(ram),0xB4u,jalrDelay));
    assert(ps2::xbox360::guest_memory::writeLittleEndian(std::span<std::uint8_t>(ram),0xC0u,jalrTarget));
    ps2::xbox360::simd::writeLane<std::uint32_t>(one,0,0xC0u); cpu.writeGpr(7,one); cpu.pc=0xB0u;
    result=step(cpu,ram); assert(result.status==StepStatus::Executed&&cpu.pc==0xB4u);
    assert(ps2::xbox360::simd::readLane<std::uint32_t>(cpu.readGpr(10),0)==0xB8u);
    result=step(cpu,ram); assert(result.status==StepStatus::Executed&&cpu.pc==0xC0u);
    result=step(cpu,ram); assert(result.status==StepStatus::Executed);
    assert(ps2::xbox360::simd::readLane<std::uint32_t>(cpu.readGpr(17),0)==34u);

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
