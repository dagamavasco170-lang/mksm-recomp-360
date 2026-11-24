#pragma once

#include "ps2_guest_memory.h"
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
    bool in_delay_slot{};
    std::uint32_t branch_pc{};
};

class PS2Runtime
{
public:
    [[nodiscard]] bool eeCheckpointDue() const noexcept { return false; }
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

// Scalar EE memory operations are explicitly little-endian after resolving a
// documented RDRAM virtual window. Vector loads/stores and special regions remain unsupported.
#define READ8(addr) \
    (::ps2::xbox360::generated_abi::readRdram<std::uint8_t>((rdram), static_cast<std::uint32_t>(addr)))
#define READ16(addr) \
    (::ps2::xbox360::generated_abi::readRdram<std::uint16_t>((rdram), static_cast<std::uint32_t>(addr)))
#define READ32(addr) \
    (::ps2::xbox360::generated_abi::readRdram<std::uint32_t>((rdram), static_cast<std::uint32_t>(addr)))
#define READ64(addr) \
    (::ps2::xbox360::generated_abi::readRdram<std::uint64_t>((rdram), static_cast<std::uint32_t>(addr)))

#define WRITE8(addr, value) \
    (::ps2::xbox360::generated_abi::writeRdram<std::uint8_t>((rdram), static_cast<std::uint32_t>(addr), static_cast<std::uint8_t>(value)))
#define WRITE16(addr, value) \
    (::ps2::xbox360::generated_abi::writeRdram<std::uint16_t>((rdram), static_cast<std::uint32_t>(addr), static_cast<std::uint16_t>(value)))
#define WRITE32(addr, value) \
    (::ps2::xbox360::generated_abi::writeRdram<std::uint32_t>((rdram), static_cast<std::uint32_t>(addr), static_cast<std::uint32_t>(value)))
#define WRITE64(addr, value) \
    (::ps2::xbox360::generated_abi::writeRdram<std::uint64_t>((rdram), static_cast<std::uint32_t>(addr), static_cast<std::uint64_t>(value)))

#define FAST_READ8(addr) READ8(addr)
#define FAST_READ16(addr) READ16(addr)
#define FAST_READ32(addr) READ32(addr)
#define FAST_READ64(addr) READ64(addr)
#define FAST_WRITE8(addr, value) WRITE8((addr), (value))
#define FAST_WRITE16(addr, value) WRITE16((addr), (value))
#define FAST_WRITE32(addr, value) WRITE32((addr), (value))
#define FAST_WRITE64(addr, value) WRITE64((addr), (value))
