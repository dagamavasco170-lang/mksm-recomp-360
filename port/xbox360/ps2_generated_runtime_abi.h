#pragma once

#include "ps2_recompiled_functions.h"

#include <array>
#include <cstdint>

struct alignas(16) Ps2GeneratedGpr128
{
    std::uint64_t low64{};
    std::uint64_t high64{};
};

// Intentionally narrow ABI for the first Xenon-generated-code probe.
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
} // namespace ps2::xbox360::generated_abi

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
