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

    simd::Vector128 prior{};
    prior.bytes[8] = 0x5A;
    cpu.writeGpr(3, prior);
    assert(executeLoadStore(cpu, ins(0x23, 1, 3, 0), ram) == LoadStoreStatus::Executed);
    assert(simd::readLane<std::uint32_t>(cpu.readGpr(3), 0) == 0x89ABCDEFu);
    assert(simd::readLane<std::uint32_t>(cpu.readGpr(3), 1) == 0xFFFFFFFFu);
    assert(cpu.readGpr(3).bytes[8] == 0x5A);

    assert(executeLoadStore(cpu, ins(0x27, 1, 6, 0), ram) == LoadStoreStatus::Executed);
    assert(simd::readLane<std::uint32_t>(cpu.readGpr(6), 0) == 0x89ABCDEFu);
    assert(simd::readLane<std::uint32_t>(cpu.readGpr(6), 1) == 0u);

    ram[40] = 0x80;
    assert(executeLoadStore(cpu, ins(0x20, 1, 4, 8), ram) == LoadStoreStatus::Executed);
    assert(simd::readLane<std::uint32_t>(cpu.readGpr(4), 0) == 0xFFFFFF80u);
    assert(simd::readLane<std::uint32_t>(cpu.readGpr(4), 1) == 0xFFFFFFFFu);
    assert(executeLoadStore(cpu, ins(0x24, 1, 5, 8), ram) == LoadStoreStatus::Executed);
    assert(simd::readLane<std::uint32_t>(cpu.readGpr(5), 0) == 0x80u);
    assert(simd::readLane<std::uint32_t>(cpu.readGpr(5), 1) == 0u);

    const auto oldPc = cpu.pc;
    const auto oldRetired = cpu.retiredInstructions;
    assert(executeLoadStore(cpu, ins(0x23, 1, 6, 1), ram) == LoadStoreStatus::AddressError);
    assert(cpu.pc == oldPc && cpu.retiredInstructions == oldRetired);
    assert(executeLoadStore(cpu, ins(0x30, 1, 6, 0), ram) == LoadStoreStatus::Unsupported);
    assert(cpu.pc == oldPc && cpu.retiredInstructions == oldRetired);

    // LQ/SQ transfer the complete 128-bit GPR and silently align the address
    // down to a 16-byte boundary (unlike scalar LH/LW/SH/SW).
    simd::Vector128 quadSource{};
    for (std::size_t i = 0; i < quadSource.bytes.size(); ++i)
        quadSource.bytes[i] = static_cast<std::uint8_t>(0xA0u + i);
    cpu.writeGpr(7, quadSource);
    simd::writeLane<std::uint32_t>(base, 0, 0x5Bu);
    cpu.writeGpr(1, base);
    assert(executeLoadStore(cpu, ins(0x1F, 1, 7, 0), ram) == LoadStoreStatus::Executed); // SQ
    assert(cpu.pc == oldPc + 4u && cpu.retiredInstructions == oldRetired + 1u);
    for (std::size_t i = 0; i < quadSource.bytes.size(); ++i)
        assert(ram[0x50u + i] == quadSource.bytes[i]);

    simd::Vector128 previousQuad{};
    previousQuad.bytes.fill(0x5Au);
    cpu.writeGpr(8, previousQuad);
    assert(executeLoadStore(cpu, ins(0x1E, 1, 8, 0), ram) == LoadStoreStatus::Executed); // LQ
    assert(cpu.pc == oldPc + 8u && cpu.retiredInstructions == oldRetired + 2u);
    assert(cpu.readGpr(8).bytes == quadSource.bytes);

    // LQ to the architectural zero register is discarded.
    cpu.writeGpr(0, previousQuad); // writeGpr must preserve GPR[0] as zero.
    assert(executeLoadStore(cpu, ins(0x1E, 1, 0, 0), ram) == LoadStoreStatus::Executed);
    assert(cpu.readGpr(0).bytes == simd::Vector128{}.bytes);

    // SQ from GPR[0] writes a full zero quadword.
    assert(executeLoadStore(cpu, ins(0x1F, 1, 0, 0x10), ram) == LoadStoreStatus::Executed);
    for (std::size_t i = 0; i < 16u; ++i)
        assert(ram[0x60u + i] == 0u);

    // LQ/SQ still reject a backing store that violates the wrapped-RAM contract.
    std::array<std::uint8_t, 255> invalidRam{};
    const auto beforeInvalidPc = cpu.pc;
    const auto beforeInvalidRetired = cpu.retiredInstructions;
    assert(executeLoadStore(cpu, ins(0x1E, 1, 9, 0), invalidRam) == LoadStoreStatus::InvalidMemory);
    assert(cpu.pc == beforeInvalidPc && cpu.retiredInstructions == beforeInvalidRetired);
}
