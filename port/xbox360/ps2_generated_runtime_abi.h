#pragma once

#include "ps2_guest_memory.h"
#include "ps2_mmi_hilo_helpers.h"
#include "ps2_rdram_address.h"
#include "ps2_spr_dma.h"
#include "ps2_iop_ioman.h"
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

    // Runtime-owned syscall bridge: generated EE code can route IOP file calls
    // through the same dispatcher used by the standalone smoke tests.
    void attachIoman(ps2::xbox360::iop::IomanDispatcher *dispatcher) noexcept
    {
        ioman_ = dispatcher;
    }

    [[nodiscard]] std::int32_t dispatchIoman(
        ps2::xbox360::iop::IomanCommand command,
        std::uint32_t a0, std::uint32_t a1, std::uint32_t a2,
        std::span<std::uint8_t> iopRam)
    {
        return ioman_ ? ioman_->dispatch(command, a0, a1, a2, iopRam) : -ENOSYS;
    }

    // EE-side wrapper: translates guest virtual pointers to bounded RDRAM offsets
    // before handing path/buffer pointers to the IOP dispatcher.
    [[nodiscard]] std::int32_t dispatchIomanEe(
        ps2::xbox360::iop::IomanCommand command,
        std::uint32_t a0, std::uint32_t a1, std::uint32_t a2,
        std::span<std::uint8_t> eeRdram)
    {
        if (!ioman_)
            return -ENOSYS;
        if (eeRdram.size() < ps2::xbox360::rdram_address::kRdramSize)
            return -EFAULT;
        switch (command)
        {
        case ps2::xbox360::iop::IomanCommand::Open:
        {
            std::uint32_t pathOffset{};
            try { pathOffset = ps2::xbox360::rdram_address::resolveOffset(a0); }
            catch (...) { return -EFAULT; }
            return ioman_->dispatch(command, pathOffset, a1, a2, eeRdram);
        }
        case ps2::xbox360::iop::IomanCommand::Read:
        {
            std::uint32_t destinationOffset{};
            try { destinationOffset = ps2::xbox360::rdram_address::resolveOffset(a1); }
            catch (...) { return -EFAULT; }
            return ioman_->dispatch(command, a0, destinationOffset, a2, eeRdram);
        }
        case ps2::xbox360::iop::IomanCommand::Close:
        case ps2::xbox360::iop::IomanCommand::Lseek:
            return ioman_->dispatch(command, a0, a1, a2, eeRdram);
        default:
            return -ENOSYS;
        }
    }

    [[nodiscard]] std::span<std::uint8_t> eeScratchpad() noexcept
    {
        return std::span<std::uint8_t>(scratchpad_);
    }

    [[nodiscard]] std::span<const std::uint8_t> eeScratchpad() const noexcept
    {
        return std::span<const std::uint8_t>(scratchpad_);
    }

    [[nodiscard]] bool readEeIo32(std::uint32_t address,std::uint32_t &value) const noexcept
    {
        return ps2::xbox360::dmac::readRegister(dmac_,address,value);
    }

    [[nodiscard]] ps2::xbox360::dmac::WriteResult writeEeIo32(
        std::span<std::uint8_t> rdram,std::uint32_t address,std::uint32_t value) noexcept
    {
        return ps2::xbox360::dmac::writeRegister(dmac_,rdram,eeScratchpad(),address,value);
    }

    // PS2Recomp emits these methods for statically classified 32-bit MMIO
    // instructions. Their definitions below delegate to the same guest-memory
    // dispatcher used by the READ32/WRITE32 ABI macros.
    [[nodiscard]] std::uint32_t Load32(std::uint8_t *rdram,
                                       R5900Context *ctx,
                                       std::uint32_t address);
    void Store32(std::uint8_t *rdram,
                 R5900Context *ctx,
                 std::uint32_t address,
                 std::uint32_t value);

private:
    // Scratchpad and the initial SPR DMAC register bank are per EE runtime.
    std::array<std::uint8_t, kScratchpadSize> scratchpad_{};
    ps2::xbox360::dmac::ControllerState dmac_{};
    ps2::xbox360::iop::IomanDispatcher *ioman_ = nullptr;
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
    const auto region=rdram_address::classifyRegion(address);
    if(region==rdram_address::RegionKind::EeIo){
        if constexpr(sizeof(UInt)==sizeof(std::uint32_t)){
            if(runtime==nullptr)throw std::invalid_argument("PS2 generated ABI: EE I/O read requires a runtime");
            const auto physical=(address>=0x80000000u&&address<0xC0000000u)?(address&0x1FFFFFFFu):address;
            std::uint32_t value{};
            if(!runtime->readEeIo32(physical,value))throw std::out_of_range("PS2 generated ABI: unsupported EE I/O register read");
            return static_cast<UInt>(value);
        }else throw std::out_of_range("PS2 generated ABI: only aligned 32-bit EE I/O reads are supported");
    }
    if(region!=rdram_address::RegionKind::Scratchpad)return readRdram<UInt>(rdram,address);
    if(runtime==nullptr)throw std::invalid_argument("PS2 generated ABI: scratchpad access requires an EE runtime");
    UInt value{};
    const auto offset=static_cast<std::size_t>(rdram_address::scratchpadOffset(address));
    if(!guest_memory::readLittleEndianBounded(runtime->eeScratchpad(),offset,value))
        throw std::out_of_range("PS2 generated ABI: scratchpad read crosses its 16 KiB boundary");
    return value;
}

template <typename UInt>
inline void writeGuestMemory(std::uint8_t *rdram,
                             PS2Runtime *runtime,
                             std::uint32_t address,
                             UInt value)
{
    if(rdram==nullptr)throw std::invalid_argument("PS2 generated ABI: null RDRAM pointer");
    const auto region=rdram_address::classifyRegion(address);
    if(region==rdram_address::RegionKind::EeIo){
        if constexpr(sizeof(UInt)==sizeof(std::uint32_t)){
            if(runtime==nullptr)throw std::invalid_argument("PS2 generated ABI: EE I/O write requires a runtime");
            const auto physical=(address>=0x80000000u&&address<0xC0000000u)?(address&0x1FFFFFFFu):address;
            const std::span<std::uint8_t> memory(rdram,kGuestRdramSize);
            const auto result=runtime->writeEeIo32(memory,physical,static_cast<std::uint32_t>(value));
            using ps2::xbox360::dmac::WriteStatus;
            switch(result.status){
            case WriteStatus::Stored:
            case WriteStatus::TransferCompleted:
            case WriteStatus::DmaDisabled:return;
            case WriteStatus::UnsupportedMode:throw std::runtime_error("PS2 generated ABI: SPR DMA mode is unsupported");
            case WriteStatus::InvalidMemory:throw std::out_of_range("PS2 generated ABI: SPR DMA memory range is invalid");
            case WriteStatus::UnsupportedRegister:throw std::out_of_range("PS2 generated ABI: unsupported EE I/O register write");
            }
        }else throw std::out_of_range("PS2 generated ABI: only aligned 32-bit EE I/O writes are supported");
    }
    if(region!=rdram_address::RegionKind::Scratchpad){writeRdram<UInt>(rdram,address,value);return;}
    if(runtime==nullptr)throw std::invalid_argument("PS2 generated ABI: scratchpad access requires an EE runtime");
    const auto offset=static_cast<std::size_t>(rdram_address::scratchpadOffset(address));
    if(!guest_memory::writeLittleEndianBounded(runtime->eeScratchpad(),offset,value))
        throw std::out_of_range("PS2 generated ABI: scratchpad write crosses its 16 KiB boundary");
}
} // namespace ps2::xbox360::generated_abi

// The generated Xbox ABI keeps each 128-bit GPR as two numeric little-endian
// 64-bit halves. These helpers bridge PS2Recomp's LQ/SQ instructions directly
// to that representation without requiring a host SIMD lane type on Xenon.
namespace
{
[[nodiscard]] inline ps2::xbox360::guest_memory::Raw128
Ps2GeneratedReadQuadword(std::uint8_t *rdram, PS2Runtime *runtime,
                         std::uint32_t address)
{
    using namespace ps2::xbox360;
    if (rdram == nullptr)
        throw std::invalid_argument("PS2 generated ABI: null RDRAM pointer");

    const std::uint32_t alignedAddress = address & ~0x0Fu;
    guest_memory::Raw128 bytes{};
    const auto region = rdram_address::classifyRegion(alignedAddress);
    if (region == rdram_address::RegionKind::Rdram)
    {
        const auto offset = generated_abi::resolveRdramOffset(alignedAddress);
        const std::span<const std::uint8_t> memory(
            rdram, generated_abi::kGuestRdramSize);
        if (!guest_memory::readRaw128(memory, offset, bytes))
            throw std::runtime_error("PS2 generated ABI: 128-bit RDRAM read failed");
        return bytes;
    }

    if (region == rdram_address::RegionKind::Scratchpad)
    {
        if (runtime == nullptr)
            throw std::invalid_argument("PS2 generated ABI: LQ scratchpad read requires an EE runtime");
        const auto scratchpad = runtime->eeScratchpad();
        const auto offset = static_cast<std::size_t>(
            rdram_address::scratchpadOffset(alignedAddress));
        if (offset > scratchpad.size() || bytes.size() > scratchpad.size() - offset)
            throw std::out_of_range("PS2 generated ABI: LQ crosses the scratchpad boundary");
        for (std::size_t i = 0; i < bytes.size(); ++i)
            bytes[i] = scratchpad[offset + i];
        return bytes;
    }

    throw std::out_of_range("PS2 generated ABI: LQ target region is unsupported");
}

inline void Ps2GeneratedWriteQuadword(
    std::uint8_t *rdram, PS2Runtime *runtime, std::uint32_t address,
    const ps2::xbox360::guest_memory::Raw128 &bytes)
{
    using namespace ps2::xbox360;
    if (rdram == nullptr)
        throw std::invalid_argument("PS2 generated ABI: null RDRAM pointer");

    const std::uint32_t alignedAddress = address & ~0x0Fu;
    const auto region = rdram_address::classifyRegion(alignedAddress);
    if (region == rdram_address::RegionKind::Rdram)
    {
        const auto offset = generated_abi::resolveRdramOffset(alignedAddress);
        const std::span<std::uint8_t> memory(
            rdram, generated_abi::kGuestRdramSize);
        if (!guest_memory::writeRaw128(memory, offset, bytes))
            throw std::runtime_error("PS2 generated ABI: 128-bit RDRAM write failed");
        return;
    }

    if (region == rdram_address::RegionKind::Scratchpad)
    {
        if (runtime == nullptr)
            throw std::invalid_argument("PS2 generated ABI: SQ scratchpad write requires an EE runtime");
        const auto scratchpad = runtime->eeScratchpad();
        const auto offset = static_cast<std::size_t>(
            rdram_address::scratchpadOffset(alignedAddress));
        if (offset > scratchpad.size() || bytes.size() > scratchpad.size() - offset)
            throw std::out_of_range("PS2 generated ABI: SQ crosses the scratchpad boundary");
        for (std::size_t i = 0; i < bytes.size(); ++i)
            scratchpad[offset + i] = bytes[i];
        return;
    }

    throw std::out_of_range("PS2 generated ABI: SQ target region is unsupported");
}
} // namespace

inline void Ps2GeneratedLq(std::uint8_t *rdram, PS2Runtime *runtime,
                           R5900Context *ctx, std::uint32_t rt,
                           std::uint32_t address)
{
    if (ctx == nullptr || rt >= ctx->r.size())
        throw std::invalid_argument("PS2 generated ABI: invalid LQ context/register");
    const auto bytes = Ps2GeneratedReadQuadword(rdram, runtime, address);
    if (rt == 0u)
        return; // The memory access still takes place for LQ r0.

    const std::span<const std::uint8_t> memory(bytes.data(), bytes.size());
    std::uint64_t low{}, high{};
    if (!ps2::xbox360::guest_memory::readLittleEndian(memory, 0u, low) ||
        !ps2::xbox360::guest_memory::readLittleEndian(memory, 8u, high))
        throw std::runtime_error("PS2 generated ABI: LQ byte decode failed");
    ctx->r[rt].low64 = low;
    ctx->r[rt].high64 = high;
}

inline void Ps2GeneratedSq(std::uint8_t *rdram, PS2Runtime *runtime,
                           R5900Context *ctx, std::uint32_t rt,
                           std::uint32_t address)
{
    if (ctx == nullptr || rt >= ctx->r.size())
        throw std::invalid_argument("PS2 generated ABI: invalid SQ context/register");

    ps2::xbox360::guest_memory::Raw128 bytes{};
    std::uint64_t low{}, high{};
    if (rt != 0u)
    {
        low = ctx->r[rt].low64;
        high = ctx->r[rt].high64;
    }
    const std::span<std::uint8_t> memory(bytes.data(), bytes.size());
    if (!ps2::xbox360::guest_memory::writeLittleEndian(memory, 0u, low) ||
        !ps2::xbox360::guest_memory::writeLittleEndian(memory, 8u, high))
        throw std::runtime_error("PS2 generated ABI: SQ byte encode failed");
    Ps2GeneratedWriteQuadword(rdram, runtime, address, bytes);
}

inline std::uint32_t PS2Runtime::Load32(std::uint8_t *rdram,
                                        R5900Context *ctx,
                                        std::uint32_t address)
{
    (void)ctx;
    return ps2::xbox360::generated_abi::readGuestMemory<std::uint32_t>(
        rdram, this, address);
}

inline void PS2Runtime::Store32(std::uint8_t *rdram,
                                R5900Context *ctx,
                                std::uint32_t address,
                                std::uint32_t value)
{
    (void)ctx;
    ps2::xbox360::generated_abi::writeGuestMemory<std::uint32_t>(
        rdram, this, address, value);
}

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
