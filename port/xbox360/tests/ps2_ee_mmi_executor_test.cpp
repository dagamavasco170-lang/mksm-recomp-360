#include "ps2_ee_dispatcher.h"

#include <array>
#include <cassert>
#include <cstdint>
#include <span>

using namespace ps2::xbox360::ee;
using ps2::xbox360::simd::Vector128;
using ps2::xbox360::simd::readLane;
using ps2::xbox360::simd::writeLane;

namespace
{
constexpr std::uint32_t encodeMmi(std::uint8_t rs, std::uint8_t rt,
                                  std::uint8_t rd, std::uint8_t sa,
                                  std::uint8_t function) noexcept
{
    return (0x1Cu << 26u) | (static_cast<std::uint32_t>(rs) << 21u) |
           (static_cast<std::uint32_t>(rt) << 16u) |
           (static_cast<std::uint32_t>(rd) << 11u) |
           (static_cast<std::uint32_t>(sa) << 6u) | function;
}

void store(std::span<std::uint8_t> ram, std::uint32_t address, std::uint32_t word)
{
    assert(ps2::xbox360::guest_memory::writeLittleEndian(ram, address, word));
}
} // namespace

int main()
{
    std::array<std::uint8_t, 256> ram{};
    CpuState cpu{};

    // PMADDH: signed halfword products accumulate across the HI/LO word banks,
    // and the destination receives LO[0], HI[0], LO[2], HI[2].
    Vector128 rs{}, rt{};
    writeLane<std::uint16_t>(rs, 0u, 2u);
    writeLane<std::uint16_t>(rs, 1u, 0xFFFDu); // -3
    writeLane<std::uint16_t>(rs, 2u, 4u);
    writeLane<std::uint16_t>(rt, 0u, 3u);
    writeLane<std::uint16_t>(rt, 1u, 6u);
    writeLane<std::uint16_t>(rt, 2u, 5u);
    cpu.writeGpr(1u, rs);
    cpu.writeGpr(2u, rt);
    store(ram, 0x00u, encodeMmi(1u, 2u, 3u, 0x10u, 0x09u)); // MMI2/PMADDH

    auto result = step(cpu, ram);
    assert(result.status == StepStatus::Executed);
    assert(cpu.pc == 4u && cpu.retiredInstructions == 1u);
    assert(readLane<std::uint32_t>(cpu.hilo.lo, 0u) == 6u);
    assert(readLane<std::uint32_t>(cpu.hilo.lo, 1u) == 0xFFFFFFEEu);
    assert(readLane<std::uint32_t>(cpu.hilo.hi, 0u) == 20u);
    assert(readLane<std::uint32_t>(cpu.readGpr(3u), 0u) == 6u);
    assert(readLane<std::uint32_t>(cpu.readGpr(3u), 1u) == 20u);

    // PMSUBH uses the same signed products and accumulator lanes as PMADDH.
    // Subtracting identical operands must restore all HI/LO words to zero.
    store(ram, 0x04u, encodeMmi(1u, 2u, 4u, 0x14u, 0x09u)); // MMI2/PMSUBH
    result = step(cpu, ram);
    assert(result.status == StepStatus::Executed);
    assert(cpu.pc == 8u && cpu.retiredInstructions == 2u);
    for (std::size_t lane = 0; lane < 4u; ++lane)
    {
        assert(readLane<std::uint32_t>(cpu.hilo.hi, lane) == 0u);
        assert(readLane<std::uint32_t>(cpu.hilo.lo, lane) == 0u);
        assert(readLane<std::uint32_t>(cpu.readGpr(4u), lane) == 0u);
    }

    // PMFHI / PMFLO copy each entire 128-bit bank to the selected GPR.
    store(ram, 0x08u, encodeMmi(0u, 0u, 4u, 0x08u, 0x09u));
    store(ram, 0x0Cu, encodeMmi(0u, 0u, 5u, 0x09u, 0x09u));
    result = step(cpu, ram);
    assert(result.status == StepStatus::Executed);
    assert(cpu.readGpr(4u).bytes == cpu.hilo.hi.bytes);
    result = step(cpu, ram);
    assert(result.status == StepStatus::Executed);
    assert(cpu.readGpr(5u).bytes == cpu.hilo.lo.bytes);

    // PMTHI / PMTLO copy a complete source GPR without aliasing the other bank.
    Vector128 newHi{}, newLo{};
    for (std::size_t i = 0; i < 16u; ++i)
    {
        newHi.bytes[i] = static_cast<std::uint8_t>(0x30u + i);
        newLo.bytes[i] = static_cast<std::uint8_t>(0xC0u - i);
    }
    cpu.writeGpr(6u, newHi);
    cpu.writeGpr(7u, newLo);
    store(ram, 0x10u, encodeMmi(6u, 0u, 0u, 0x08u, 0x29u));
    store(ram, 0x14u, encodeMmi(7u, 0u, 0u, 0x09u, 0x29u));
    result = step(cpu, ram);
    assert(result.status == StepStatus::Executed);
    assert(cpu.hilo.hi.bytes == newHi.bytes);
    assert(cpu.hilo.lo.bytes != newLo.bytes);
    result = step(cpu, ram);
    assert(result.status == StepStatus::Executed);
    assert(cpu.hilo.lo.bytes == newLo.bytes);
    assert(cpu.hilo.hi.bytes == newHi.bytes);

    // MMI2 PAND/PXOR and MMI3 POR/PNOR operate on all 128 bits.
    Vector128 bitA{}, bitB{};
    for (std::size_t i = 0; i < 16u; ++i)
    {
        bitA.bytes[i] = static_cast<std::uint8_t>(0xF0u | (i & 0x0Fu));
        bitB.bytes[i] = static_cast<std::uint8_t>(0x0Fu ^ (i & 0x07u));
    }
    cpu.writeGpr(11u, bitA);
    cpu.writeGpr(12u, bitB);
    store(ram, 0x18u, encodeMmi(11u, 12u, 13u, 0x12u, 0x09u)); // PAND
    result = step(cpu, ram);
    assert(result.status == StepStatus::Executed);
    for (std::size_t i = 0; i < 16u; ++i)
        assert(cpu.readGpr(13u).bytes[i] == (bitA.bytes[i] & bitB.bytes[i]));
    store(ram, 0x1Cu, encodeMmi(11u, 12u, 14u, 0x13u, 0x09u)); // PXOR
    result = step(cpu, ram);
    assert(result.status == StepStatus::Executed);
    for (std::size_t i = 0; i < 16u; ++i)
        assert(cpu.readGpr(14u).bytes[i] == (bitA.bytes[i] ^ bitB.bytes[i]));
    store(ram, 0x20u, encodeMmi(11u, 12u, 15u, 0x12u, 0x29u)); // POR
    result = step(cpu, ram);
    assert(result.status == StepStatus::Executed);
    for (std::size_t i = 0; i < 16u; ++i)
        assert(cpu.readGpr(15u).bytes[i] == (bitA.bytes[i] | bitB.bytes[i]));
    store(ram, 0x24u, encodeMmi(11u, 12u, 16u, 0x13u, 0x29u)); // PNOR
    result = step(cpu, ram);
    assert(result.status == StepStatus::Executed);
    for (std::size_t i = 0; i < 16u; ++i)
        assert(cpu.readGpr(16u).bytes[i] == static_cast<std::uint8_t>(~(bitA.bytes[i] | bitB.bytes[i])));
    
    // PMTHL.LW writes words 0/2 into LO and HI while preserving words 1/3.
    Vector128 pmthlSource{};
    writeLane<std::uint32_t>(pmthlSource, 0u, 0xDEADBEEFu);
    writeLane<std::uint32_t>(pmthlSource, 1u, 0xCAFEBABEu);
    writeLane<std::uint32_t>(pmthlSource, 2u, 0x0BADF00Du);
    writeLane<std::uint32_t>(pmthlSource, 3u, 0x8BADF00Du);
    cpu.writeGpr(17u, pmthlSource);
    const auto preservedLo1 = readLane<std::uint32_t>(cpu.hilo.lo, 1u);
    const auto preservedLo3 = readLane<std::uint32_t>(cpu.hilo.lo, 3u);
    const auto preservedHi1 = readLane<std::uint32_t>(cpu.hilo.hi, 1u);
    const auto preservedHi3 = readLane<std::uint32_t>(cpu.hilo.hi, 3u);
    store(ram, 0x28u, encodeMmi(17u, 0u, 0u, 0x00u, 0x31u)); // PMTHL.LW
    result = step(cpu, ram);
    assert(result.status == StepStatus::Executed && cpu.pc == 0x2Cu);
    assert(readLane<std::uint32_t>(cpu.hilo.lo, 0u) == 0xDEADBEEFu);
    assert(readLane<std::uint32_t>(cpu.hilo.lo, 1u) == preservedLo1);
    assert(readLane<std::uint32_t>(cpu.hilo.lo, 2u) == 0x0BADF00Du);
    assert(readLane<std::uint32_t>(cpu.hilo.lo, 3u) == preservedLo3);
    assert(readLane<std::uint32_t>(cpu.hilo.hi, 0u) == 0xCAFEBABEu);
    assert(readLane<std::uint32_t>(cpu.hilo.hi, 1u) == preservedHi1);
    assert(readLane<std::uint32_t>(cpu.hilo.hi, 2u) == 0x8BADF00Du);
    assert(readLane<std::uint32_t>(cpu.hilo.hi, 3u) == preservedHi3);

    // PMFHL.LW reads the same four selected words into a GPR.
    store(ram, 0x2Cu, encodeMmi(0u, 0u, 18u, 0x00u, 0x30u)); // PMFHL.LW
    result = step(cpu, ram);
    assert(result.status == StepStatus::Executed && cpu.pc == 0x30u);
    assert(readLane<std::uint32_t>(cpu.readGpr(18u), 0u) == 0xDEADBEEFu);
    assert(readLane<std::uint32_t>(cpu.readGpr(18u), 1u) == 0xCAFEBABEu);
    assert(readLane<std::uint32_t>(cpu.readGpr(18u), 2u) == 0x0BADF00Du);
    assert(readLane<std::uint32_t>(cpu.readGpr(18u), 3u) == 0x8BADF00Du);

    // A supported MMI operation in a branch delay slot must retire, then branch.
    const std::uint32_t beq = (0x04u << 26u) | (1u << 21u) | (1u << 16u) | 1u;
    store(ram, 0x30u, beq);
    store(ram, 0x34u, encodeMmi(0u, 0u, 8u, 0x08u, 0x09u)); // PMFHI in delay slot
    store(ram, 0x38u, 0x24090001u); // ADDIU r9, r0, 1
    Vector128 one{};
    writeLane<std::uint32_t>(one, 0u, 1u);
    cpu.writeGpr(1u, one);
    cpu.pc = 0x30u;
    result = step(cpu, ram);
    assert(result.status == StepStatus::Executed && cpu.pc == 0x34u && cpu.branchDelayPending);
    result = step(cpu, ram);
    assert(result.status == StepStatus::Executed && cpu.pc == 0x38u && !cpu.branchDelayPending);
    assert(cpu.readGpr(8u).bytes == cpu.hilo.hi.bytes);
    result = step(cpu, ram);
    assert(result.status == StepStatus::Executed);
    assert(readLane<std::uint32_t>(cpu.readGpr(9u), 0u) == 1u);

    // Unimplemented MMI operations are explicitly rejected without retiring.
    store(ram, 0x3Cu, encodeMmi(1u, 2u, 10u, 0x1Fu, 0x09u));
    const auto beforePc = cpu.pc;
    const auto beforeRetired = cpu.retiredInstructions;
    const auto beforeHi = cpu.hilo.hi.bytes;
    const auto beforeLo = cpu.hilo.lo.bytes;
    result = step(cpu, ram);
    assert(result.status == StepStatus::Unsupported);
    assert(cpu.pc == beforePc && cpu.retiredInstructions == beforeRetired);
    assert(cpu.hilo.hi.bytes == beforeHi && cpu.hilo.lo.bytes == beforeLo);

    // MMI0 PADDW/PSUBW use wrapping arithmetic independently in each 32-bit lane.
    Vector128 wordA{}, wordB{};
    writeLane<std::uint32_t>(wordA, 0u, 0xFFFFFFFFu);
    writeLane<std::uint32_t>(wordA, 1u, 0x7FFFFFFFu);
    writeLane<std::uint32_t>(wordA, 2u, 0x80000000u);
    writeLane<std::uint32_t>(wordA, 3u, 0x12345678u);
    writeLane<std::uint32_t>(wordB, 0u, 0x00000001u);
    writeLane<std::uint32_t>(wordB, 1u, 0x00000001u);
    writeLane<std::uint32_t>(wordB, 2u, 0xFFFFFFFFu);
    writeLane<std::uint32_t>(wordB, 3u, 0x87654321u);
    cpu.writeGpr(19u, wordA);
    cpu.writeGpr(20u, wordB);
    cpu.pc = 0x40u;
    store(ram, 0x40u, encodeMmi(19u, 20u, 21u, 0x00u, 0x08u)); // MMI0/PADDW
    result = step(cpu, ram);
    assert(result.status == StepStatus::Executed && cpu.pc == 0x44u);
    assert(readLane<std::uint32_t>(cpu.readGpr(21u), 0u) == 0u);
    assert(readLane<std::uint32_t>(cpu.readGpr(21u), 1u) == 0x80000000u);
    assert(readLane<std::uint32_t>(cpu.readGpr(21u), 2u) == 0x7FFFFFFFu);
    assert(readLane<std::uint32_t>(cpu.readGpr(21u), 3u) == 0x99999999u);
    store(ram, 0x44u, encodeMmi(19u, 20u, 22u, 0x01u, 0x08u)); // MMI0/PSUBW
    result = step(cpu, ram);
    assert(result.status == StepStatus::Executed && cpu.pc == 0x48u);
    assert(readLane<std::uint32_t>(cpu.readGpr(22u), 0u) == 0xFFFFFFFEu);
    assert(readLane<std::uint32_t>(cpu.readGpr(22u), 1u) == 0x7FFFFFFEu);
    assert(readLane<std::uint32_t>(cpu.readGpr(22u), 2u) == 0x80000001u);
    assert(readLane<std::uint32_t>(cpu.readGpr(22u), 3u) == 0x88888888u);

    return 0;
}
