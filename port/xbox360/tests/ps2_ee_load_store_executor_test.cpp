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

    // R5900 LWL/LWR merge selected bytes from an unaligned effective
    // address with the containing aligned word. LWL always sign-extends the
    // merged low word; LWR only does so for offset zero.
    std::array<std::uint8_t, 256> mergeRam{};
    CpuState mergeCpu{};
    simd::Vector128 mergeBase{}, mergeInitial{};
    simd::writeLane<std::uint32_t>(mergeBase, 0u, 0x40u);
    simd::writeLane<std::uint32_t>(mergeInitial, 0u, 0xAABBCCDDu);
    simd::writeLane<std::uint32_t>(mergeInitial, 1u, 0x55667788u);
    simd::writeLane<std::uint32_t>(mergeInitial, 2u, 0x10203040u);
    simd::writeLane<std::uint32_t>(mergeInitial, 3u, 0x50607080u);
    mergeCpu.writeGpr(1u, mergeBase);
    assert(guest_memory::writeLittleEndian(std::span<std::uint8_t>(mergeRam), 0x40u, 0xF2332211u));

    constexpr std::array<std::uint32_t, 4> expectedLwl{
        0x11BBCCDDu, 0x2211CCDDu, 0x332211DDu, 0xF2332211u};
    constexpr std::array<std::uint32_t, 4> expectedLwr{
        0xF2332211u, 0xAAF23322u, 0xAABBF233u, 0xAABBCCF2u};
    for (std::uint16_t offset = 0u; offset < 4u; ++offset)
    {
        mergeCpu.writeGpr(5u, mergeInitial);
        assert(executeLoadStore(mergeCpu, ins(0x22u, 1u, 5u, offset), mergeRam) ==
               LoadStoreStatus::Executed); // LWL
        assert(simd::readLane<std::uint32_t>(mergeCpu.readGpr(5u), 0u) ==
               expectedLwl[offset]);
        assert(simd::readLane<std::uint32_t>(mergeCpu.readGpr(5u), 1u) ==
               (offset == 3u ? 0xFFFFFFFFu : 0u));
        assert(simd::readLane<std::uint32_t>(mergeCpu.readGpr(5u), 2u) ==
               0x10203040u);
        assert(simd::readLane<std::uint32_t>(mergeCpu.readGpr(5u), 3u) ==
               0x50607080u);

        mergeCpu.writeGpr(5u, mergeInitial);
        assert(executeLoadStore(mergeCpu, ins(0x26u, 1u, 5u, offset), mergeRam) ==
               LoadStoreStatus::Executed); // LWR
        assert(simd::readLane<std::uint32_t>(mergeCpu.readGpr(5u), 0u) ==
               expectedLwr[offset]);
        assert(simd::readLane<std::uint32_t>(mergeCpu.readGpr(5u), 1u) ==
               (offset == 0u ? 0xFFFFFFFFu : 0x55667788u));
        assert(simd::readLane<std::uint32_t>(mergeCpu.readGpr(5u), 2u) ==
               0x10203040u);
        assert(simd::readLane<std::uint32_t>(mergeCpu.readGpr(5u), 3u) ==
               0x50607080u);
    }

    // SWL/SWR update only the addressed byte lanes of the containing word.
    constexpr std::array<std::uint32_t, 4> expectedSwl{
        0x443322AAu, 0x4433AABBu, 0x44AABBCCu, 0xAABBCCDDu};
    constexpr std::array<std::uint32_t, 4> expectedSwr{
        0xAABBCCDDu, 0xBBCCDD11u, 0xCCDD2211u, 0xDD332211u};
    simd::Vector128 mergeSource{};
    simd::writeLane<std::uint32_t>(mergeSource, 0u, 0xAABBCCDDu);
    mergeCpu.writeGpr(2u, mergeSource);
    for (std::uint16_t offset = 0u; offset < 4u; ++offset)
    {
        assert(guest_memory::writeLittleEndian(std::span<std::uint8_t>(mergeRam), 0x40u, 0x44332211u));
        assert(executeLoadStore(mergeCpu, ins(0x2Au, 1u, 2u, offset), mergeRam) ==
               LoadStoreStatus::Executed); // SWL
        std::uint32_t stored{};
        assert(guest_memory::readLittleEndian(std::span<const std::uint8_t>(mergeRam), 0x40u, stored));
        assert(stored == expectedSwl[offset]);

        assert(guest_memory::writeLittleEndian(std::span<std::uint8_t>(mergeRam), 0x40u, 0x44332211u));
        assert(executeLoadStore(mergeCpu, ins(0x2Eu, 1u, 2u, offset), mergeRam) ==
               LoadStoreStatus::Executed); // SWR
        assert(guest_memory::readLittleEndian(std::span<const std::uint8_t>(mergeRam), 0x40u, stored));
        assert(stored == expectedSwr[offset]);
    }

    // The zero register still participates in the memory access, but loads
    // discard their result and stores merge zero bits into the selected lanes.
    assert(guest_memory::writeLittleEndian(std::span<std::uint8_t>(mergeRam), 0x40u, 0x44332211u));
    assert(executeLoadStore(mergeCpu, ins(0x22u, 1u, 0u, 0u), mergeRam) ==
           LoadStoreStatus::Executed);
    assert(mergeCpu.readGpr(0u).bytes == simd::Vector128{}.bytes);
    assert(executeLoadStore(mergeCpu, ins(0x2Au, 1u, 0u, 0u), mergeRam) ==
           LoadStoreStatus::Executed);
    std::uint32_t zeroStoreWord{};
    assert(guest_memory::readLittleEndian(std::span<const std::uint8_t>(mergeRam), 0x40u, zeroStoreWord));
    assert(zeroStoreWord == 0x44332200u);

    // Invalid backing memory must fail without retiring a merge instruction.
    std::array<std::uint8_t, 255> invalidMergeRam{};
    const auto mergePcBeforeInvalid = mergeCpu.pc;
    const auto mergeRetiredBeforeInvalid = mergeCpu.retiredInstructions;
    assert(executeLoadStore(mergeCpu, ins(0x26u, 1u, 5u, 0u), invalidMergeRam) ==
           LoadStoreStatus::InvalidMemory);
    assert(mergeCpu.pc == mergePcBeforeInvalid &&
           mergeCpu.retiredInstructions == mergeRetiredBeforeInvalid);

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
