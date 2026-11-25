#include "ps2_ee_dispatcher.h"
#include <array>
#include <cassert>
#include <cstdint>
using namespace ps2::xbox360::ee;
static std::uint32_t cop0(std::uint8_t rs,std::uint8_t rt,std::uint8_t rd,std::uint8_t funct=0u){return (0x10u<<26)|(std::uint32_t(rs)<<21)|(std::uint32_t(rt)<<16)|(std::uint32_t(rd)<<11)|funct;}
int main(){
 std::array<std::uint8_t,128> ram{}; CpuState cpu{}; cpu.pc=0x10;
 auto write=[&](std::uint32_t pc,std::uint32_t instr){assert(ps2::xbox360::guest_memory::writeLittleEndian(std::span<std::uint8_t>(ram),pc,instr));};
 cpu.cop0.status=0x1234; write(0x10,cop0(0,5,12)); auto r=step(cpu,ram); assert(r.status==StepStatus::Executed&&cpu.pc==0x14); assert(ps2::xbox360::simd::readLane<std::uint32_t>(cpu.readGpr(5),0)==0x1234);
 ps2::xbox360::simd::Vector128 value{}; ps2::xbox360::simd::writeLane<std::uint32_t>(value,0,CpuState::StatusExl|0x40u); cpu.writeGpr(6,value); cpu.pc=0x20; write(0x20,cop0(4,6,12)); r=step(cpu,ram); assert(r.status==StepStatus::Executed&&cpu.cop0.status==(CpuState::StatusExl|0x40u));
 ps2::xbox360::simd::writeLane<std::uint32_t>(value,0,0x80001000u); cpu.writeGpr(7,value); cpu.pc=0x24; write(0x24,cop0(4,7,14)); r=step(cpu,ram); assert(r.status==StepStatus::Executed&&cpu.cop0.epc==0x80001000u);
 cpu.cop0.cause=0x80000000u; cpu.pc=0x28; write(0x28,cop0(0,8,13)); r=step(cpu,ram); assert(r.status==StepStatus::Executed); assert(ps2::xbox360::simd::readLane<std::uint64_t>(cpu.readGpr(8),0)==0xFFFFFFFF80000000ull);
 cpu.pc=0x2C; cpu.cop0.status|=CpuState::StatusExl; cpu.branchDelayPending=true; cpu.branchTarget=0x55; write(0x2C,cop0(0x10,0,0,0x18)); r=step(cpu,ram); assert(r.status==StepStatus::Executed&&cpu.pc==0x80001000u); assert((cpu.cop0.status&CpuState::StatusExl)==0u&&!cpu.branchDelayPending&&cpu.branchTarget==0u);
 cpu.pc=0x30; cpu.cop0.status&=~CpuState::StatusExl; write(0x30,cop0(0x10,0,0,0x18)); const auto retired=cpu.retiredInstructions; r=step(cpu,ram); assert(r.status==StepStatus::Unsupported&&cpu.pc==0x30&&cpu.retiredInstructions==retired);
 return 0;
}
