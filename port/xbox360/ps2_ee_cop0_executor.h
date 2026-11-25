#pragma once
#include "ps2_ee_cpu_state.h"
#include "ps2_ee_instruction_decode.h"
#include <cstdint>
namespace ps2::xbox360::ee
{
enum class Cop0Status : std::uint8_t { Executed, Unsupported };
[[nodiscard]] constexpr std::uint32_t readCop0(const CpuState &state, std::uint8_t reg) noexcept
{
    switch (reg) { case 8: return state.cop0.badVaddr; case 12: return state.cop0.status; case 13: return state.cop0.cause; case 14: return state.cop0.epc; default: return 0u; }
}
[[nodiscard]] constexpr Cop0Status executeCop0(CpuState &state, const DecodedInstruction &d) noexcept
{
    if (d.kind != InstructionKind::Cop0) return Cop0Status::Unsupported;
    if (d.rs == 0x10u && d.funct == 0x18u) // ERET
    {
        if ((state.cop0.status & CpuState::StatusExl) == 0u) return Cop0Status::Unsupported;
        state.cop0.status &= ~CpuState::StatusExl; state.pc = state.cop0.epc;
        state.branchDelayPending = false; state.branchTarget = 0u; state.recordRetiredInstruction();
        return Cop0Status::Executed;
    }
    if (d.rs == 0u) // MFC0, word sign-extended
    {
        const std::uint32_t value = readCop0(state, d.rd);
        const std::uint64_t extended = static_cast<std::uint64_t>(static_cast<std int64_t>(static_cast<std::int32_t>(value)));
        auto out = state.gpr[d.rt]; for (std::size_t i=0;i<8u;++i) out.bytes[i]=static_cast<std::uint8_t>(extended>>(i*8u));
        state.writeGpr(d.rt,out); state.advancePc(); state.recordRetiredInstruction(); return Cop0Status::Executed;
    }
    if (d.rs == 4u) // MTC0
    {
        const auto value=simd::readLane<std::uint32_t>(state.readGpr(d.rt),0);
        switch(d.rd) { case 12: state.cop0.status=value; break; case 13: state.cop0.cause=(state.cop0.cause&~0x300u)|(value&0x300u); break; case 14: state.cop0.epc=value; break; default: return Cop0Status::Unsupported; }
        state.advancePc(); state.recordRetiredInstruction(); return Cop0Status::Executed;
    }
    return Cop0Status::Unsupported;
}
} // namespace ps2::xbox360::ee
