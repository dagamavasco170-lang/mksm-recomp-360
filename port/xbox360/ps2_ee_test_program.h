#pragma once

#include "ps2_ee_dispatcher.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

namespace ps2::xbox360::ee::test_program
{
struct TraceReport
{
    bool passed{};
    std::uint64_t signature{};
    std::uint32_t failureMask{};
    std::uint32_t pc{};
    std::uint64_t retiredInstructions{};
};

namespace detail
{
constexpr std::uint32_t iType(std::uint8_t opcode, std::uint8_t rs,
                              std::uint8_t rt, std::uint16_t immediate) noexcept
{
    return (static_cast<std::uint32_t>(opcode) << 26u) |
           (static_cast<std::uint32_t>(rs) << 21u) |
           (static_cast<std::uint32_t>(rt) << 16u) | immediate;
}

constexpr std::uint32_t rType(std::uint8_t rs, std::uint8_t rt,
                              std::uint8_t rd, std::uint8_t function) noexcept
{
    return (static_cast<std::uint32_t>(rs) << 21u) |
           (static_cast<std::uint32_t>(rt) << 16u) |
           (static_cast<std::uint32_t>(rd) << 11u) | function;
}

constexpr std::uint32_t mmi(std::uint8_t rs, std::uint8_t rt,
                            std::uint8_t rd, std::uint8_t sa,
                            std::uint8_t function) noexcept
{
    return (0x1Cu << 26u) |
           (static_cast<std::uint32_t>(rs) << 21u) |
           (static_cast<std::uint32_t>(rt) << 16u) |
           (static_cast<std::uint32_t>(rd) << 11u) |
           (static_cast<std::uint32_t>(sa) << 6u) | function;
}

inline void hashByte(std::uint64_t &hash, std::uint8_t byte) noexcept
{
    hash ^= byte;
    hash *= 1099511628211ull;
}

inline void hashU32(std::uint64_t &hash, std::uint32_t value) noexcept
{
    for (unsigned shift = 0; shift < 32u; shift += 8u)
        hashByte(hash, static_cast<std::uint8_t>(value >> shift));
}

inline void hashU64(std::uint64_t &hash, std::uint64_t value) noexcept
{
    for (unsigned shift = 0; shift < 64u; shift += 8u)
        hashByte(hash, static_cast<std::uint8_t>(value >> shift));
}
} // namespace detail

// A small data-only EE program shared by the host regression runner and the
// Xbox 360 smoke title. It covers arithmetic, little-endian RAM, a taken branch
// with a delay slot, HI/LO transfers and the supported PMADDH path.
[[nodiscard]] inline TraceReport runDeterministicTrace() noexcept
{
    using namespace detail;
    using simd::readLane;

    std::array<std::uint8_t, 256> ram{};
    CpuState cpu{};

    // 00 LUI r1,1234       04 ORI r1,r1,5678   08 ADDIU r2,r1,-1
    // 0C XORI r3,r2,00FF   10 SW r3,80(r0)     14 LW r4,80(r0)
    // 18 ADDU r5,r3,r4     1C BEQ r3,r4,+2     20 delay: ADDIU r6,r0,9
    // 24 skipped: ADDIU r7,r0,99              28 target: ADDIU r7,r6,3
    // 2C PMTHI r5          30 PMTLO r4         34 PMFHI r8
    // 38 PMFLO r9          3C PMADDH r10,r1,r2 40 SW r10,90(r0)
    // 44 LW r11,90(r0)
    constexpr std::array<std::uint32_t, 18> program{
        iType(0x0Fu, 0u, 1u, 0x1234u),
        iType(0x0Du, 1u, 1u, 0x5678u),
        iType(0x09u, 1u, 2u, 0xFFFFu),
        iType(0x0Eu, 2u, 3u, 0x00FFu),
        iType(0x2Bu, 0u, 3u, 0x0080u),
        iType(0x23u, 0u, 4u, 0x0080u),
        rType(3u, 4u, 5u, 0x21u),
        iType(0x04u, 3u, 4u, 0x0002u),
        iType(0x09u, 0u, 6u, 0x0009u),
        iType(0x09u, 0u, 7u, 0x0063u),
        iType(0x09u, 6u, 7u, 0x0003u),
        mmi(5u, 0u, 0u, 0x08u, 0x29u), // PMTHI
        mmi(4u, 0u, 0u, 0x09u, 0x29u), // PMTLO
        mmi(0u, 0u, 8u, 0x08u, 0x09u), // PMFHI
        mmi(0u, 0u, 9u, 0x09u, 0x09u), // PMFLO
        mmi(1u, 2u, 10u, 0x10u, 0x09u), // PMADDH
        iType(0x2Bu, 0u, 10u, 0x0090u),
        iType(0x23u, 0u, 11u, 0x0090u)
    };

    const auto memory = std::span<std::uint8_t>(ram);
    for (std::size_t i = 0; i < program.size(); ++i)
    {
        if (!guest_memory::writeLittleEndian(
                memory, static_cast<std::uint32_t>(i * sizeof(std::uint32_t)), program[i]))
            return {false, 0u, 0x80000000u, cpu.pc, cpu.retiredInstructions};
    }

    constexpr std::uint64_t kExpectedRetired = 17u;
    for (std::uint64_t i = 0; i < kExpectedRetired; ++i)
    {
        const auto result = step(cpu, memory);
        if (result.status != StepStatus::Executed)
            return {false, 0u, 0x80000000u, cpu.pc, cpu.retiredInstructions};
    }

    std::uint32_t storedAt80{};
    std::uint32_t storedAt90{};
    const bool memoryMatches =
        guest_memory::readLittleEndian(std::span<const std::uint8_t>(ram), 0x80u, storedAt80) &&
        guest_memory::readLittleEndian(std::span<const std::uint8_t>(ram), 0x90u, storedAt90);

    std::uint32_t failureMask = 0u;
    const bool controlMatches =
        cpu.pc == 0x48u && cpu.retiredInstructions == kExpectedRetired &&
        !cpu.branchDelayPending;
    const bool basicRegistersMatch =
        readLane<std::uint32_t>(cpu.readGpr(0u), 0u) == 0u &&
        readLane<std::uint32_t>(cpu.readGpr(1u), 0u) == 0x12345678u &&
        readLane<std::uint32_t>(cpu.readGpr(2u), 0u) == 0x12345677u &&
        readLane<std::uint32_t>(cpu.readGpr(3u), 0u) == 0x12345688u &&
        readLane<std::uint32_t>(cpu.readGpr(4u), 0u) == 0x12345688u &&
        readLane<std::uint32_t>(cpu.readGpr(5u), 0u) == 0x2468AD10u &&
        readLane<std::uint32_t>(cpu.readGpr(6u), 0u) == 9u &&
        readLane<std::uint32_t>(cpu.readGpr(7u), 0u) == 12u;
    const bool hiloTransfersMatch =
        cpu.readGpr(8u).bytes == cpu.hilo.hi.bytes &&
        cpu.readGpr(9u).bytes == cpu.hilo.lo.bytes;
    const bool pmaddhRegistersMatch =
        readLane<std::uint32_t>(cpu.readGpr(10u), 0u) == 0x2F68D850u &&
        readLane<std::uint32_t>(cpu.readGpr(10u), 1u) == 0x2468AD10u &&
        readLane<std::uint32_t>(cpu.readGpr(10u), 2u) == 0u &&
        readLane<std::uint32_t>(cpu.readGpr(10u), 3u) == 0u &&
        readLane<std::uint32_t>(cpu.readGpr(11u), 0u) == 0x2F68D850u;
    const bool hiloAccumulatorMatches =
        cpu.hilo.loWord(0u) == 0x2F68D850u &&
        cpu.hilo.loWord(1u) == 0x014B5A90u &&
        cpu.hilo.hiWord(0u) == 0x2468AD10u;
    const bool memoryValuesMatch =
        memoryMatches && storedAt80 == 0x12345688u && storedAt90 == 0x2F68D850u;

    if (!controlMatches) failureMask |= 1u << 0u;
    if (!basicRegistersMatch) failureMask |= 1u << 1u;
    if (!hiloTransfersMatch) failureMask |= 1u << 2u;
    if (!pmaddhRegistersMatch) failureMask |= 1u << 3u;
    if (!hiloAccumulatorMatches) failureMask |= 1u << 4u;
    if (!memoryValuesMatch) failureMask |= 1u << 5u;

    std::uint64_t hash = 14695981039346656037ull;
    for (const auto &gpr : cpu.gpr)
        for (const auto byte : gpr.bytes)
            hashByte(hash, byte);
    for (const auto byte : cpu.hilo.hi.bytes)
        hashByte(hash, byte);
    for (const auto byte : cpu.hilo.lo.bytes)
        hashByte(hash, byte);
    hashU32(hash, cpu.pc);
    hashU64(hash, cpu.retiredInstructions);
    hashByte(hash, cpu.branchDelayPending ? 1u : 0u);
    hashU32(hash, cpu.branchTarget);
    for (std::size_t i = 0x80u; i < 0x94u; ++i)
        hashByte(hash, ram[i]);

    return {failureMask == 0u, hash, failureMask, cpu.pc, cpu.retiredInstructions};
}
} // namespace ps2::xbox360::ee::test_program
