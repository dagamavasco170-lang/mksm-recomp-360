#pragma once

#include "ps2_ee_cpu_state.h"
#include "ps2_ee_instruction_decode.h"
#include "ps2_guest_memory.h"

#include <cstdint>
#include <span>

namespace ps2::xbox360::ee
{
enum class LoadStoreStatus : std::uint8_t
{
    Executed,
    Unsupported,
    AddressError,
    InvalidMemory
};

// Conservative scalar EE memory slice. Addresses are guest physical offsets into
// the supplied wrapped-RAM span; this is not a virtual-memory/MMIO implementation.
[[nodiscard]] inline LoadStoreStatus executeLoadStore(
    CpuState &state, const DecodedInstruction &d, std::span<std::uint8_t> memory) noexcept
{
    if (d.kind != InstructionKind::LoadStore)
        return LoadStoreStatus::Unsupported;

    const std::uint32_t base = simd::readLane<std::uint32_t>(state.readGpr(d.rs), 0);
    const auto displacement = static_cast<std::int32_t>(
        std::bit_cast<std::int16_t>(d.immediate));
    const std::uint32_t address = base + static_cast<std::uint32_t>(displacement);

    // R5900 unaligned word merges access the containing aligned word and
    // merge only the addressed byte lanes. They do not raise scalar address
    // alignment exceptions.
    if (d.opcode == 0x22u || d.opcode == 0x26u ||
        d.opcode == 0x2Au || d.opcode == 0x2Eu) // LWL/LWR/SWL/SWR
    {
        if (!guest_memory::validWrappedRam(memory))
            return LoadStoreStatus::InvalidMemory;

        const std::uint32_t alignedAddress = address & ~0x03u;
        const std::uint32_t offset = address & 0x03u;
        std::uint32_t memoryWord{};
        if (!guest_memory::readLittleEndian(
                std::span<const std::uint8_t>(memory), alignedAddress, memoryWord))
            return LoadStoreStatus::InvalidMemory;

        if (d.opcode == 0x22u || d.opcode == 0x26u) // LWL / LWR
        {
            if (d.rt != 0u)
            {
                simd::Vector128 result = state.readGpr(d.rt);
                const std::uint32_t oldLow =
                    simd::readLane<std::uint32_t>(result, 0u);
                std::uint32_t merged{};
                bool signExtendLow64 = false;

                if (d.opcode == 0x22u) // LWL
                {
                    const std::uint32_t shift = (3u - offset) * 8u;
                    const std::uint32_t keepMask =
                        shift == 0u ? 0u : ((std::uint32_t{1u} << shift) - 1u);
                    merged = (oldLow & keepMask) | (memoryWord << shift);
                    signExtendLow64 = true;
                }
                else // LWR
                {
                    const std::uint32_t shift = offset * 8u;
                    const std::uint32_t keepMask =
                        shift == 0u ? 0u : (0xFFFFFFFFu << (32u - shift));
                    merged = (oldLow & keepMask) | (memoryWord >> shift);
                    signExtendLow64 = offset == 0u;
                }

                simd::writeLane<std::uint32_t>(result, 0u, merged);
                if (signExtendLow64)
                {
                    const std::uint32_t fill =
                        (merged & 0x80000000u) != 0u ? 0xFFFFFFFFu : 0u;
                    simd::writeLane<std::uint32_t>(result, 1u, fill);
                }
                state.writeGpr(d.rt, result);
            }
        }
        else // SWL / SWR
        {
            const std::uint32_t value =
                simd::readLane<std::uint32_t>(state.readGpr(d.rt), 0u);
            std::uint32_t merged{};
            if (d.opcode == 0x2Au) // SWL
            {
                const std::uint32_t shift = (3u - offset) * 8u;
                const std::uint32_t keepMask =
                    shift == 0u ? 0u : (0xFFFFFFFFu << (32u - shift));
                merged = (memoryWord & keepMask) | (value >> shift);
            }
            else // SWR
            {
                const std::uint32_t shift = offset * 8u;
                const std::uint32_t keepMask =
                    shift == 0u ? 0u : ((std::uint32_t{1u} << shift) - 1u);
                merged = (memoryWord & keepMask) | (value << shift);
            }
            if (!guest_memory::writeLittleEndian(memory, alignedAddress, merged))
                return LoadStoreStatus::InvalidMemory;
        }

        state.advancePc();
        state.recordRetiredInstruction();
        return LoadStoreStatus::Executed;
    }

    // R5900 LQ/SQ silently align the effective address down to 16 bytes.
    // They transfer all 128 guest bytes and do not raise the scalar alignment
    // exception used by LB/LH/LW and SB/SH/SW.
    if (d.opcode == 0x1Eu || d.opcode == 0x1Fu)
    {
        if (!guest_memory::validWrappedRam(memory))
            return LoadStoreStatus::InvalidMemory;

        const std::uint32_t alignedAddress = address & ~0x0Fu;
        if (d.opcode == 0x1Eu) // LQ
        {
            guest_memory::Raw128 raw{};
            if (!guest_memory::readRaw128(memory, alignedAddress, raw))
                return LoadStoreStatus::InvalidMemory;

            simd::Vector128 value{};
            value.bytes = raw;
            state.writeGpr(d.rt, value);
        }
        else // SQ
        {
            const guest_memory::Raw128 raw = state.readGpr(d.rt).bytes;
            if (!guest_memory::writeRaw128(memory, alignedAddress, raw))
                return LoadStoreStatus::InvalidMemory;
        }

        state.advancePc();
        state.recordRetiredInstruction();
        return LoadStoreStatus::Executed;
    }

    std::size_t width = 0;
    bool load = false;
    bool signExtend = false;
    switch (d.opcode)
    {
    case 0x20: width = 1; load = true; signExtend = true; break; // LB
    case 0x24: width = 1; load = true; break;                    // LBU
    case 0x21: width = 2; load = true; signExtend = true; break; // LH
    case 0x25: width = 2; load = true; break;                    // LHU
    case 0x23: width = 4; load = true; signExtend = true; break;  // LW
    case 0x27: width = 4; load = true; break;                    // LWU
    case 0x28: width = 1; break;                                 // SB
    case 0x29: width = 2; break;                                 // SH
    case 0x2B: width = 4; break;                                 // SW
    default: return LoadStoreStatus::Unsupported;
    }

    if ((address & static_cast<std::uint32_t>(width - 1u)) != 0u)
        return LoadStoreStatus::AddressError;
    if (!guest_memory::validWrappedRam(memory))
        return LoadStoreStatus::InvalidMemory;

    if (load)
    {
        std::uint32_t value = 0;
        bool ok = false;
        if (width == 1)
        {
            std::uint8_t v{};
            ok = guest_memory::readLittleEndian(memory, address, v);
            value = signExtend ? static_cast<std::uint32_t>(
                static_cast<std::int32_t>(std::bit_cast<std::int8_t>(v))) : v;
        }
        else if (width == 2)
        {
            std::uint16_t v{};
            ok = guest_memory::readLittleEndian(memory, address, v);
            value = signExtend ? static_cast<std::uint32_t>(
                static_cast<std::int32_t>(std::bit_cast<std::int16_t>(v))) : v;
        }
        else
        {
            ok = guest_memory::readLittleEndian(memory, address, value);
        }
        if (!ok)
            return LoadStoreStatus::InvalidMemory;

        // EE scalar loads update GPR[rt][63:0], not the upper 64-bit MMI half.
        // LW/LB/LH sign-extend to 64 bits; unsigned forms zero-extend.
        const std::uint64_t scalar = signExtend
            ? static_cast<std::uint64_t>(static_cast<std::int64_t>(
                  std::bit_cast<std::int32_t>(value)))
            : static_cast<std::uint64_t>(value);
        simd::Vector128 result = state.readGpr(d.rt);
        for (std::size_t byte = 0; byte < sizeof(scalar); ++byte)
            result.bytes[byte] = static_cast<std::uint8_t>(scalar >> (byte * 8u));
        state.writeGpr(d.rt, result);
    }
    else
    {
        const std::uint32_t value =
            simd::readLane<std::uint32_t>(state.readGpr(d.rt), 0);
        bool ok = false;
        if (width == 1)
            ok = guest_memory::writeLittleEndian(memory, address,
                                                  static_cast<std::uint8_t>(value));
        else if (width == 2)
            ok = guest_memory::writeLittleEndian(memory, address,
                                                  static_cast<std::uint16_t>(value));
        else
            ok = guest_memory::writeLittleEndian(memory, address, value);
        if (!ok)
            return LoadStoreStatus::InvalidMemory;
    }

    state.advancePc();
    state.recordRetiredInstruction();
    return LoadStoreStatus::Executed;
}
} // namespace ps2::xbox360::ee
