#include "ps2_ee_dispatcher.h"

#include <array>
#include <bit>
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
    assert(readLane<std::uint32_t>(cpu.readGpr(22u), 3u) == 0x8ACF1357u);

    // MMI0 PCGTW and PMAXW use signed comparisons across four word lanes.
    cpu.pc = 0x48u;
    store(ram, 0x48u, encodeMmi(19u, 20u, 23u, 0x02u, 0x08u)); // PCGTW
    result = step(cpu, ram);
    assert(result.status == StepStatus::Executed && cpu.pc == 0x4Cu);
    assert(readLane<std::uint32_t>(cpu.readGpr(23u), 0u) == 0u);
    assert(readLane<std::uint32_t>(cpu.readGpr(23u), 1u) == 0xFFFFFFFFu);
    assert(readLane<std::uint32_t>(cpu.readGpr(23u), 2u) == 0u);
    assert(readLane<std::uint32_t>(cpu.readGpr(23u), 3u) == 0xFFFFFFFFu);
    store(ram, 0x4Cu, encodeMmi(19u, 20u, 24u, 0x03u, 0x08u)); // PMAXW
    result = step(cpu, ram);
    assert(result.status == StepStatus::Executed && cpu.pc == 0x50u);
    assert(readLane<std::uint32_t>(cpu.readGpr(24u), 0u) == 1u);
    assert(readLane<std::uint32_t>(cpu.readGpr(24u), 1u) == 0x7FFFFFFFu);
    assert(readLane<std::uint32_t>(cpu.readGpr(24u), 2u) == 0xFFFFFFFFu);
    assert(readLane<std::uint32_t>(cpu.readGpr(24u), 3u) == 0x12345678u);

    // MMI1 PCEQW and PMINW: equality masks and signed minima.
    Vector128 equalVector = wordB;
    writeLane<std::uint32_t>(equalVector, 0u, 0xFFFFFFFFu);
    cpu.writeGpr(25u, equalVector);
    cpu.pc = 0x50u;
    store(ram, 0x50u, encodeMmi(19u, 25u, 26u, 0x02u, 0x28u)); // PCEQW
    result = step(cpu, ram);
    assert(result.status == StepStatus::Executed && cpu.pc == 0x54u);
    assert(readLane<std::uint32_t>(cpu.readGpr(26u), 0u) == 0xFFFFFFFFu);
    assert(readLane<std::uint32_t>(cpu.readGpr(26u), 1u) == 0u);
    assert(readLane<std::uint32_t>(cpu.readGpr(26u), 2u) == 0u);
    assert(readLane<std::uint32_t>(cpu.readGpr(26u), 3u) == 0u);
    store(ram, 0x54u, encodeMmi(19u, 20u, 27u, 0x03u, 0x28u)); // PMINW
    result = step(cpu, ram);
    assert(result.status == StepStatus::Executed && cpu.pc == 0x58u);
    assert(readLane<std::uint32_t>(cpu.readGpr(27u), 0u) == 0xFFFFFFFFu);
    assert(readLane<std::uint32_t>(cpu.readGpr(27u), 1u) == 1u);
    assert(readLane<std::uint32_t>(cpu.readGpr(27u), 2u) == 0x80000000u);
    assert(readLane<std::uint32_t>(cpu.readGpr(27u), 3u) == 0x87654321u);

    // MMI1 unsigned saturating arithmetic clamps at lane-width bounds.
    Vector128 satUnsignedA{}, satUnsignedB{};
    writeLane<std::uint32_t>(satUnsignedA, 0u, 0xFFFFFFFFu);
    writeLane<std::uint32_t>(satUnsignedA, 1u, 3u);
    writeLane<std::uint32_t>(satUnsignedA, 2u, 0u);
    writeLane<std::uint32_t>(satUnsignedA, 3u, 0x80000000u);
    writeLane<std::uint32_t>(satUnsignedB, 0u, 1u);
    writeLane<std::uint32_t>(satUnsignedB, 1u, 5u);
    writeLane<std::uint32_t>(satUnsignedB, 2u, 0xFFFFFFFFu);
    writeLane<std::uint32_t>(satUnsignedB, 3u, 0x80000000u);
    cpu.writeGpr(1u, satUnsignedA);
    cpu.writeGpr(2u, satUnsignedB);
    cpu.pc = 0x54u;
    store(ram, 0x54u, encodeMmi(1u, 2u, 3u, 0x10u, 0x28u)); // PADDUW
    result = step(cpu, ram);
    assert(result.status == StepStatus::Executed && cpu.pc == 0x58u);
    assert(readLane<std::uint32_t>(cpu.readGpr(3u), 0u) == 0xFFFFFFFFu);
    assert(readLane<std::uint32_t>(cpu.readGpr(3u), 1u) == 8u);
    assert(readLane<std::uint32_t>(cpu.readGpr(3u), 2u) == 0xFFFFFFFFu);
    assert(readLane<std::uint32_t>(cpu.readGpr(3u), 3u) == 0xFFFFFFFFu);
    store(ram, 0x58u, encodeMmi(1u, 2u, 4u, 0x11u, 0x28u)); // PSUBUW
    result = step(cpu, ram);
    assert(result.status == StepStatus::Executed && cpu.pc == 0x5Cu);
    assert(readLane<std::uint32_t>(cpu.readGpr(4u), 0u) == 0xFFFFFFFEu);
    assert(readLane<std::uint32_t>(cpu.readGpr(4u), 1u) == 0u);
    assert(readLane<std::uint32_t>(cpu.readGpr(4u), 2u) == 0u);
    assert(readLane<std::uint32_t>(cpu.readGpr(4u), 3u) == 0u);

    Vector128 satHalfA{}, satHalfB{};
    writeLane<std::uint16_t>(satHalfA, 0u, 0xFFFFu);
    writeLane<std::uint16_t>(satHalfA, 1u, 4u);
    writeLane<std::uint16_t>(satHalfB, 0u, 1u);
    writeLane<std::uint16_t>(satHalfB, 1u, 5u);
    cpu.writeGpr(1u, satHalfA);
    cpu.writeGpr(2u, satHalfB);
    store(ram, 0x5Cu, encodeMmi(1u, 2u, 5u, 0x14u, 0x28u)); // PADDUH
    result = step(cpu, ram);
    assert(result.status == StepStatus::Executed && cpu.pc == 0x60u);
    assert(readLane<std::uint16_t>(cpu.readGpr(5u), 0u) == 0xFFFFu);
    assert(readLane<std::uint16_t>(cpu.readGpr(5u), 1u) == 9u);
    store(ram, 0x60u, encodeMmi(1u, 2u, 6u, 0x15u, 0x28u)); // PSUBUH
    result = step(cpu, ram);
    assert(result.status == StepStatus::Executed && cpu.pc == 0x64u);
    assert(readLane<std::uint16_t>(cpu.readGpr(6u), 0u) == 0xFFFEu);
    assert(readLane<std::uint16_t>(cpu.readGpr(6u), 1u) == 0u);

    Vector128 satByteA{}, satByteB{};
    satByteA.bytes[0] = 0xFFu; satByteA.bytes[1] = 4u;
    satByteB.bytes[0] = 1u; satByteB.bytes[1] = 5u;
    cpu.writeGpr(1u, satByteA);
    cpu.writeGpr(2u, satByteB);
    store(ram, 0x64u, encodeMmi(1u, 2u, 7u, 0x18u, 0x28u)); // PADDUB
    result = step(cpu, ram);
    assert(result.status == StepStatus::Executed && cpu.pc == 0x68u);
    assert(cpu.readGpr(7u).bytes[0] == 0xFFu && cpu.readGpr(7u).bytes[1] == 9u);
    store(ram, 0x68u, encodeMmi(1u, 2u, 8u, 0x19u, 0x28u)); // PSUBUB
    result = step(cpu, ram);
    assert(result.status == StepStatus::Executed && cpu.pc == 0x6Cu);
    assert(cpu.readGpr(8u).bytes[0] == 0xFEu && cpu.readGpr(8u).bytes[1] == 0u);

    // MMI1 PABSW saturates INT_MIN to INT_MAX without an exception.
    Vector128 absWordInput{};
    writeLane<std::uint32_t>(absWordInput, 0u, 0x80000000u);
    writeLane<std::uint32_t>(absWordInput, 1u, 0xFFFFFFFDu);
    writeLane<std::uint32_t>(absWordInput, 2u, 0x7FFFFFFFu);
    writeLane<std::uint32_t>(absWordInput, 3u, 0u);
    cpu.writeGpr(1u, absWordInput);
    cpu.pc = 0x5Cu;
    store(ram, 0x5Cu, encodeMmi(0u, 1u, 2u, 0x01u, 0x28u)); // PABSW
    result = step(cpu, ram);
    assert(result.status == StepStatus::Executed && cpu.pc == 0x60u);
    assert(readLane<std::uint32_t>(cpu.readGpr(2u), 0u) == 0x7FFFFFFFu);
    assert(readLane<std::uint32_t>(cpu.readGpr(2u), 1u) == 3u);
    assert(readLane<std::uint32_t>(cpu.readGpr(2u), 2u) == 0x7FFFFFFFu);
    assert(readLane<std::uint32_t>(cpu.readGpr(2u), 3u) == 0u);

    // MMI1 PABSH and PCEQB: signed absolute values (including -32768)
    // and equality masks across sixteen byte lanes.
    Vector128 absInput{}, byteA{}, byteB{};
    const std::array<std::uint16_t, 8> absValues{0x8000u, 0xFFFFu, 0x7FFFu, 0x0000u,
                                                    0xFFFEu, 0x1234u, 0xFEDCu, 0x0100u};
    for (std::size_t lane = 0; lane < 8u; ++lane)
        writeLane<std::uint16_t>(absInput, lane, absValues[lane]);
    cpu.writeGpr(1u, absInput);
    cpu.pc = 0x60u;
    store(ram, 0x60u, encodeMmi(1u, 0u, 2u, 0x05u, 0x28u)); // PABSH
    result = step(cpu, ram);
    assert(result.status == StepStatus::Executed && cpu.pc == 0x64u);
    const std::array<std::uint16_t, 8> absExpected{0x8000u, 1u, 0x7FFFu, 0u,
                                                     2u, 0x1234u, 0x0124u, 0x0100u};
    for (std::size_t lane = 0; lane < 8u; ++lane)
        assert(readLane<std::uint16_t>(cpu.readGpr(2u), lane) == absExpected[lane]);
    for (std::size_t lane = 0; lane < 16u; ++lane)
    {
        byteA.bytes[lane] = static_cast<std::uint8_t>((lane * 13u + 0x35u) & 0xFFu);
        byteB.bytes[lane] = static_cast<std::uint8_t>((lane % 3u == 0u)
            ? byteA.bytes[lane] : (byteA.bytes[lane] ^ 0xFFu));
    }
    cpu.writeGpr(3u, byteA);
    cpu.writeGpr(4u, byteB);
    store(ram, 0x64u, encodeMmi(3u, 4u, 5u, 0x0Au, 0x28u)); // PCEQB
    result = step(cpu, ram);
    assert(result.status == StepStatus::Executed && cpu.pc == 0x68u);
    for (std::size_t lane = 0; lane < 16u; ++lane)
        assert(cpu.readGpr(5u).bytes[lane] ==
               (byteA.bytes[lane] == byteB.bytes[lane] ? 0xFFu : 0u));

    // MMI0 PADDSW/PSUBSW saturate signed 32-bit lanes at both bounds.
    Vector128 satA{}, satB{};
    writeLane<std::uint32_t>(satA, 0u, 0x7FFFFFFFu);
    writeLane<std::uint32_t>(satA, 1u, 0x80000000u);
    writeLane<std::uint32_t>(satA, 2u, 0x00000010u);
    writeLane<std::uint32_t>(satA, 3u, 0xFFFFFFF0u);
    writeLane<std::uint32_t>(satB, 0u, 1u);
    writeLane<std::uint32_t>(satB, 1u, 0xFFFFFFFFu);
    writeLane<std::uint32_t>(satB, 2u, 0xFFFFFFF0u);
    writeLane<std::uint32_t>(satB, 3u, 0x00000010u);
    cpu.writeGpr(6u, satA);
    cpu.writeGpr(7u, satB);
    cpu.pc = 0x70u;
    store(ram, 0x70u, encodeMmi(6u, 7u, 8u, 0x10u, 0x08u)); // PADDSW
    result = step(cpu, ram);
    assert(result.status == StepStatus::Executed && cpu.pc == 0x74u);
    assert(readLane<std::uint32_t>(cpu.readGpr(8u), 0u) == 0x7FFFFFFFu);
    assert(readLane<std::uint32_t>(cpu.readGpr(8u), 1u) == 0x80000000u);
    assert(readLane<std::uint32_t>(cpu.readGpr(8u), 2u) == 0u);
    assert(readLane<std::uint32_t>(cpu.readGpr(8u), 3u) == 0u);
    store(ram, 0x74u, encodeMmi(6u, 7u, 9u, 0x11u, 0x08u)); // PSUBSW
    result = step(cpu, ram);
    assert(result.status == StepStatus::Executed && cpu.pc == 0x78u);
    assert(readLane<std::uint32_t>(cpu.readGpr(9u), 0u) == 0x7FFFFFFEu);
    assert(readLane<std::uint32_t>(cpu.readGpr(9u), 1u) == 0x80000001u);
    assert(readLane<std::uint32_t>(cpu.readGpr(9u), 2u) == 0x20u);
    assert(readLane<std::uint32_t>(cpu.readGpr(9u), 3u) == 0xFFFFFFE0u);

    // MMI0 PADDH/PSUBH and PADDB/PSUBB wrap independently in narrow lanes.
    Vector128 narrowA{}, narrowB{};
    for (std::size_t lane = 0; lane < 8u; ++lane)
    {
        writeLane<std::uint16_t>(narrowA, lane, static_cast<std::uint16_t>(0xFFF0u + lane));
        writeLane<std::uint16_t>(narrowB, lane, static_cast<std::uint16_t>(0x0020u + lane));
    }
    cpu.writeGpr(10u, narrowA);
    cpu.writeGpr(11u, narrowB);
    cpu.pc = 0x80u;
    store(ram, 0x80u, encodeMmi(10u, 11u, 12u, 0x04u, 0x08u)); // PADDH
    result = step(cpu, ram);
    assert(result.status == StepStatus::Executed && cpu.pc == 0x84u);
    for (std::size_t lane = 0; lane < 8u; ++lane)
        assert(readLane<std::uint16_t>(cpu.readGpr(12u), lane) ==
               static_cast<std::uint16_t>(0xFFF0u + lane + 0x0020u + lane));
    store(ram, 0x84u, encodeMmi(10u, 11u, 13u, 0x05u, 0x08u)); // PSUBH
    result = step(cpu, ram);
    assert(result.status == StepStatus::Executed && cpu.pc == 0x88u);
    for (std::size_t lane = 0; lane < 8u; ++lane)
        assert(readLane<std::uint16_t>(cpu.readGpr(13u), lane) ==
               static_cast<std::uint16_t>(0xFFF0u + lane - (0x0020u + lane)));

    for (std::size_t lane = 0; lane < 16u; ++lane)
    {
        narrowA.bytes[lane] = static_cast<std::uint8_t>(0xF8u + lane);
        narrowB.bytes[lane] = static_cast<std::uint8_t>(0x10u + lane * 3u);
    }
    cpu.writeGpr(10u, narrowA);
    cpu.writeGpr(11u, narrowB);
    store(ram, 0x88u, encodeMmi(10u, 11u, 14u, 0x06u, 0x08u)); // PADDB
    result = step(cpu, ram);
    assert(result.status == StepStatus::Executed && cpu.pc == 0x8Cu);
    for (std::size_t lane = 0; lane < 16u; ++lane)
        assert(cpu.readGpr(14u).bytes[lane] ==
               static_cast<std::uint8_t>(narrowA.bytes[lane] + narrowB.bytes[lane]));
    store(ram, 0x8Cu, encodeMmi(10u, 11u, 15u, 0x07u, 0x08u)); // PSUBB
    result = step(cpu, ram);
    assert(result.status == StepStatus::Executed && cpu.pc == 0x90u);
    for (std::size_t lane = 0; lane < 16u; ++lane)
        assert(cpu.readGpr(15u).bytes[lane] ==
               static_cast<std::uint8_t>(narrowA.bytes[lane] - narrowB.bytes[lane]));

    return 0;
}
