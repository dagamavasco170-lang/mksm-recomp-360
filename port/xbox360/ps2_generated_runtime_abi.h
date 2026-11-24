#pragma once

#include "ps2_guest_memory.h"
#include "ps2_mmi_hilo_helpers.h"
#include "ps2_rdram_address.h"
#include "ps2_recompiled_functions.h"

#include <array>
#include <cstdint>
#include <span>
#include <stdexcept>

struct alignas(16) Ps2GeneratedGpr128
{
    std::uint64_t low64{};
    std::uint64_t high64{};
};

// Intentionally narrow ABI for the first Xenon-generated-code probes.
// It is not a replacement for the desktop R5900Context.
struct R5900Context
{
    std::array<Ps2GeneratedGpr128, 32> r{};
    std::uint32_t pc{};
    std::uint64_t hi{};
    std::uint64_t lo{};
    std::uint64_t hi1{};
    std::uint64_t lo1{};
    bool in_delay_slot{};
    std::uint32_t branch_pc{};
};

// Match the desktop runtime's split 128-bit MMI HI/LO banks.
inline ps2::mmi::HiLoState Ps2MmiReadHiLo(const R5900Context *ctx) noexcept
{
    if (ctx == nullptr)
        return {};
    return {ps2::mmi::from64Halves(ctx->hi, ctx->hi1),
            ps2::mmi::from64Halves(ctx->lo, ctx->lo1)};
}

inline void Ps2MmiWriteHiLo(R5900Context *ctx,
                            const ps2::mmi::HiLoState &state) noexcept
{
    if (ctx == nullptr)
        return;
    ps2::mmi::to64Halves(state.hi, ctx->hi, ctx->hi1);
    ps2::mmi::to64Halves(state.lo, ctx->lo, ctx->lo1);
}

inline ps2::mmi::Vector128 Ps2MmiReadGprVector(const R5900Context *ctx,
                                               std::uint32_t reg) noexcept
{
    ps2::mmi::Vector128 result{};
    if (ctx == nullptr || reg == 0u || reg >= ctx->r.size())
        return result;
    const auto &entry = ctx->r[reg];
    result.words[0] = static_cast<std::uint32_t>(entry.low64);
    result.words[1] = static_cast<std::uint32_t>(entry.low64 >> 32u);
    result.words[2] = static_cast<std::uint32_t>(entry.high64);
    result.words[3] = static_cast<std::uint32_t>(entry.high64 >> 32u);
    return result;
}

inline void Ps2MmiWriteGprVector(R5900Context *ctx, std::uint32_t reg,
                                 const ps2::mmi::Vector128 &value) noexcept
{
    if (ctx == nullptr || reg == 0u || reg >= ctx->r.size())
        return;
    auto &entry = ctx->r[reg];
    entry.low64 = static_cast<std::uint64_t>(value.words[0]) |
                  (static_cast<std::uint64_t>(value.words[1]) << 32u);
    entry.high64 = static_cast<std::uint64_t>(value.words[2]) |
                   (static_cast<std::uint64_t>(value.words[3]) << 32u);
}

inline void Ps2MmiPmfhi(R5900Context *ctx, std::uint32_t rd) noexcept
{
    Ps2MmiWriteGprVector(ctx, rd, ps2::mmi::pmfhi(Ps2MmiReadHiLo(ctx)));
}

inline void Ps2MmiPmflo(R5900Context *ctx, std::uint32_t rd) noexcept
{
    Ps2MmiWriteGprVector(ctx, rd, ps2::mmi::pmflo(Ps2MmiReadHiLo(ctx)));
}

inline void Ps2MmiPmthi(R5900Context *ctx, std::uint32_t rs) noexcept
{
    auto state = Ps2MmiReadHiLo(ctx);
    ps2::mmi::pmthi(state, Ps2MmiReadGprVector(ctx, rs));
    Ps2MmiWriteHiLo(ctx, state);
}

inline void Ps2MmiPmtlo(R5900Context *ctx, std::uint32_t rs) noexcept
{
    auto state = Ps2MmiReadHiLo(ctx);
    ps2::mmi::pmtlo(state, Ps2MmiReadGprVector(ctx, rs));
    Ps2MmiWriteHiLo(ctx, state);
}

inline void Ps2MmiPmaddh(R5900Context *ctx,
                         std::uint32_t rs,
                         std::uint32_t rt,
                         std::uint32_t rd) noexcept
{
    auto state = Ps2MmiReadHiLo(ctx);
    const auto destination = ps2::mmi::pmaddh(
        state, Ps2MmiReadGprVector(ctx, rs), Ps2MmiReadGprVector(ctx, rt));
    Ps2MmiWriteHiLo(ctx, state);
    Ps2MmiWriteGprVector(ctx, rd, destination);
}

inline void Ps2MmiPmsubh(R5900Context *ctx,
                         std::uint32_t rs,
                         std::uint32_t rt,
                         std::uint32_t rd) noexcept
{
    auto state = Ps2MmiReadHiLo(ctx);
    const auto destination = ps2::mmi::pmsubh(
        state, Ps2MmiReadGprVector(ctx, rs), Ps2MmiReadGprVector(ctx, rt));
    Ps2MmiWriteHiLo(ctx, state);
    Ps2MmiWriteGprVector(ctx, rd, destination);
}

inline void Ps2MmiPmfhlLw(R5900Context *ctx, std::uint32_t rd) noexcept
{
    Ps2MmiWriteGprVector(ctx, rd, ps2::mmi::pmfhlLw(Ps2MmiReadHiLo(ctx)));
}

inline void Ps2MmiPmthlLw(R5900Context *ctx, std::uint32_t rs) noexcept
{
    auto state = Ps2MmiReadHiLo(ctx);
    ps2::mmi::pmthlLw(state, Ps2MmiReadGprVector(ctx, rs));
    Ps2MmiWriteHiLo(ctx, state);
}

class PS2Runtime
{
public:
    static constexpr std::size_t kScratchpadSize =
        ps2::xbox360::rdram_address::kScratchpadSize;

    [[nodiscard]] bool eeCheckpointDue() const noexcept { return false; }

    [[nodiscard]] std::span<std::uint8_t> eeScratchpad() noexcept
    {
        return std::span<std::uint8_t>(scratchpad_);
    }

    [[nodiscard]] std::span<const std::uint8_t> eeScratchpad() const noexcept
    {
        return std::span<const std::uint8_t>(scratchpad_);
    }

private:
    // Scratchpad is per EE runtime and deliberately separate from main RDRAM.
    std::array<std::uint8_t, kScratchpadSize> scratchpad_{};
};

namespace ps2::xbox360::generated_abi
{
inline constexpr std::size_t kGuestRdramSize = 32u * 1024u * 1024u;
inline constexpr std::uint32_t kGuestRdramMask =
    static_cast<std::uint32_t>(kGuestRdramSize - 1u);

[[nodiscard]] inline std::uint32_t gprU32(const R5900Context *ctx,
                                           std::uint32_t index) noexcept
{
    return (ctx == nullptr || index == 0u || index >= ctx->r.size())
               ? 0u
               : static_cast<std::uint32_t>(ctx->r[index].low64);
}

[[nodiscard]] inline std::uint64_t gprU64(const R5900Context *ctx,
                                           std::uint32_t index) noexcept
{
    return (ctx == nullptr || index == 0u || index >= ctx->r.size())
               ? 0u
               : ctx->r[index].low64;
}

inline void setGprLow64(R5900Context *ctx, std::uint32_t index,
                        std::uint64_t value) noexcept
{
    if (ctx != nullptr && index != 0u && index < ctx->r.size())
        ctx->r[index].low64 = value;
}

// Keep virtual-window decoding separate from endian-safe byte access so the
// accepted aliases can be tested without pulling in generated code.
[[nodiscard]] inline std::uint32_t resolveRdramOffset(std::uint32_t address)
{
    static_assert(kGuestRdramSize == rdram_address::kRdramSize);
    return rdram_address::resolveOffset(address);
}

template <typename UInt>
[[nodiscard]] inline UInt readRdram(const std::uint8_t *rdram,
                                    std::uint32_t address)
{
    if (rdram == nullptr)
        throw std::invalid_argument("PS2 generated ABI: null RDRAM pointer");

    UInt value{};
    const auto offset = resolveRdramOffset(address);
    const std::span<const std::uint8_t> memory(rdram, kGuestRdramSize);
    if (!guest_memory::readLittleEndian(memory, offset, value))
        throw std::runtime_error("PS2 generated ABI: little-endian RDRAM read failed");
    return value;
}

template <typename UInt>
inline void writeRdram(std::uint8_t *rdram, std::uint32_t address, UInt value)
{
    if (rdram == nullptr)
        throw std::invalid_argument("PS2 generated ABI: null RDRAM pointer");

    const auto offset = resolveRdramOffset(address);
    const std::span<std::uint8_t> memory(rdram, kGuestRdramSize);
    if (!guest_memory::writeLittleEndian(memory, offset, value))
        throw std::runtime_error("PS2 generated ABI: little-endian RDRAM write failed");
}

template <typename UInt>
[[nodiscard]] inline UInt readGuestMemory(const std::uint8_t *rdram,
                                          PS2Runtime *runtime,
                                          std::uint32_t address)
{
    if (rdram == nullptr)
        throw std::invalid_argument("PS2 generated ABI: null RDRAM pointer");

    if (rdram_address::classifyRegion(address) != rdram_address::RegionKind::Scratchpad)
        return readRdram<UInt>(rdram, address);

    if (runtime == nullptr)
        throw std::invalid_argument("PS2 generated ABI: scratchpad access requires an EE runtime");

    UInt value{};
    const auto offset = static_cast<std::size_t>(
        rdram_address::scratchpadOffset(address));
    if (!guest_memory::readLittleEndianBounded(runtime->eeScratchpad(), offset, value))
        throw std::out_of_range("PS2 generated ABI: scratchpad read crosses its 16 KiB boundary");
    return value;
}

template <typename UInt>
inline void writeGuestMemory(std::uint8_t *rdram,
                             PS2Runtime *runtime,
                             std::uint32_t address,
                             UInt value)
{
    if (rdram == nullptr)
        throw std::invalid_argument("PS2 generated ABI: null RDRAM pointer");

    if (rdram_address::classifyRegion(address) != rdram_address::RegionKind::Scratchpad)
    {
        writeRdram<UInt>(rdram, address, value);
        return;
    }

    if (runtime == nullptr)
        throw std::invalid_argument("PS2 generated ABI: scratchpad access requires an EE runtime");

    const auto offset = static_cast<std::size_t>(
        rdram_address::scratchpadOffset(address));
    if (!guest_memory::writeLittleEndianBounded(runtime->eeScratchpad(), offset, value))
        throw std::out_of_range("PS2 generated ABI: scratchpad write crosses its 16 KiB boundary");
}
} // namespace ps2::xbox360::generated_abi

// 32-bit EE arithmetic wraps modulo 2^32 on ADDIU/ADDU.
#define ADD32(lhs, rhs) \
    (static_cast<std::uint32_t>(static_cast<std::uint32_t>(lhs) + static_cast<std::uint32_t>(rhs)))

#define GPR_U32(ctx_ptr, reg_idx) \
    (::ps2::xbox360::generated_abi::gprU32((ctx_ptr), static_cast<std::uint32_t>(reg_idx)))
#define GPR_U64(ctx_ptr, reg_idx) \
    (::ps2::xbox360::generated_abi::gprU64((ctx_ptr), static_cast<std::uint32_t>(reg_idx)))

#define SET_GPR_U32(ctx_ptr, reg_idx, value)                                      \
    do                                                                             \
    {                                                                              \
        const std::uint32_t _ps2_reg = static_cast<std::uint32_t>(reg_idx);        \
        const std::uint32_t _ps2_value = static_cast<std::uint32_t>(value);        \
        ::ps2::xbox360::generated_abi::setGprLow64(                                \
            (ctx_ptr), _ps2_reg, static_cast<std::uint64_t>(                       \
                static_cast<std::int64_t>(static_cast<std::int32_t>(_ps2_value))));\
    } while (0)

#define SET_GPR_S32(ctx_ptr, reg_idx, value) SET_GPR_U32((ctx_ptr), (reg_idx), (value))

#define SET_GPR_U64(ctx_ptr, reg_idx, value)                                      \
    do                                                                             \
    {                                                                              \
        const std::uint32_t _ps2_reg = static_cast<std::uint32_t>(reg_idx);        \
        const std::uint64_t _ps2_value = static_cast<std::uint64_t>(value);        \
        ::ps2::xbox360::generated_abi::setGprLow64((ctx_ptr), _ps2_reg, _ps2_value);\
    } while (0)

#define SET_GPR_S64(ctx_ptr, reg_idx, value) SET_GPR_U64((ctx_ptr), (reg_idx), (value))

// Scalar EE memory operations route RDRAM windows to little-endian RDRAM and
// the 16 KiB scratchpad to separate per-runtime storage. Other special regions
// remain explicitly unsupported.
#define READ8(addr) \
    (::ps2::xbox360::generated_abi::readGuestMemory<std::uint8_t>((rdram), (runtime), static_cast<std::uint32_t>(addr)))
#define READ16(addr) \
    (::ps2::xbox360::generated_abi::readGuestMemory<std::uint16_t>((rdram), (runtime), static_cast<std::uint32_t>(addr)))
#define READ32(addr) \
    (::ps2::xbox360::generated_abi::readGuestMemory<std::uint32_t>((rdram), (runtime), static_cast<std::uint32_t>(addr)))
#define READ64(addr) \
    (::ps2::xbox360::generated_abi::readGuestMemory<std::uint64_t>((rdram), (runtime), static_cast<std::uint32_t>(addr)))

#define WRITE8(addr, value) \
    (::ps2::xbox360::generated_abi::writeGuestMemory<std::uint8_t>((rdram), (runtime), static_cast<std::uint32_t>(addr), static_cast<std::uint8_t>(value)))
#define WRITE16(addr, value) \
    (::ps2::xbox360::generated_abi::writeGuestMemory<std::uint16_t>((rdram), (runtime), static_cast<std::uint32_t>(addr), static_cast<std::uint16_t>(value)))
#define WRITE32(addr, value) \
    (::ps2::xbox360::generated_abi::writeGuestMemory<std::uint32_t>((rdram), (runtime), static_cast<std::uint32_t>(addr), static_cast<std::uint32_t>(value)))
#define WRITE64(addr, value) \
    (::ps2::xbox360::generated_abi::writeGuestMemory<std::uint64_t>((rdram), (runtime), static_cast<std::uint32_t>(addr), static_cast<std::uint64_t>(value)))

#define FAST_READ8(addr) READ8(addr)
#define FAST_READ16(addr) READ16(addr)
#define FAST_READ32(addr) READ32(addr)
#define FAST_READ64(addr) READ64(addr)
#define FAST_WRITE8(addr, value) WRITE8((addr), (value))
#define FAST_WRITE16(addr, value) WRITE16((addr), (value))
#define FAST_WRITE32(addr, value) WRITE32((addr), (value))
#define FAST_WRITE64(addr, value) WRITE64((addr), (value))
