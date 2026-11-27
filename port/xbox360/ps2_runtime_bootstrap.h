#pragma once

#include "ps2_generated_runtime_abi.h"
#include "ps2_iop_ioman_trace_ring.h"
#include "ps2_recompiled_block_dispatcher.h"

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <span>
#include <vector>

namespace ps2::xbox360
{
enum class GeneratedEntryStatus : std::uint8_t
{
    Invoked,
    NullEntry
};

// Owns the narrow Xenon generated-code execution boundary: 32 MiB EE RDRAM,
// generated ABI context, IOMAN dispatcher and metadata trace. Game-specific
// syscall IDs and the generated entry function are supplied by verified
// title/build metadata; this class does not invent either.
class PS2RuntimeBootstrap
{
public:
    static constexpr std::size_t EeRdramSize = rdram_address::kRdramSize;

    explicit PS2RuntimeBootstrap(const std::filesystem::path &discRoot)
        : rdram_(EeRdramSize), ioman_(discRoot)
    {
        runtime_.attachIomanTrace(&trace_);
        runtime_.attachIoman(&ioman_);
    }

    PS2RuntimeBootstrap(const PS2RuntimeBootstrap &) = delete;
    PS2RuntimeBootstrap &operator=(const PS2RuntimeBootstrap &) = delete;

    [[nodiscard]] bool bindIomanSyscall(std::uint32_t syscallId,
                                        iop::IomanCommand command) noexcept
    {
        return runtime_.bindIomanSyscall(syscallId, command);
    }

    [[nodiscard]] std::int32_t dispatchEeIomanSyscall(
        std::uint32_t syscallId, std::uint32_t a0, std::uint32_t a1,
        std::uint32_t a2)
    {
        return runtime_.dispatchEeIomanSyscall(syscallId, a0, a1, a2, rdram_);
    }

    // Convenience bridge for the EE O32-style argument registers:
    // a0-a2 are GPR4-GPR6 and the signed return value is written to v0/GPR2.
    // The syscall ID is explicit because the title's syscall encoding/table
    // has not yet been established from a verified executable.
    [[nodiscard]] std::int32_t dispatchIomanFromContext(std::uint32_t syscallId)
    {
        const auto a0 = static_cast<std::uint32_t>(context_.r[4].low64);
        const auto a1 = static_cast<std::uint32_t>(context_.r[5].low64);
        const auto a2 = static_cast<std::uint32_t>(context_.r[6].low64);
        const auto result = dispatchEeIomanSyscall(syscallId, a0, a1, a2);
        context_.r[2].low64 = static_cast<std::uint64_t>(static_cast<std::int64_t>(result));
        return result;
    }

    // Invoke one host-generated recompiled entry/basic block with the owned
    // guest memory and context. Scheduling/chaining between blocks remains the
    // caller's responsibility until a verified generated-function map exists.
    [[nodiscard]] GeneratedEntryStatus invokeGeneratedEntry(
        PS2Runtime::RecompiledFunction entry)
    {
        if (!entry)
            return GeneratedEntryStatus::NullEntry;
        entry(rdram_.data(), &context_, &runtime_);
        return GeneratedEntryStatus::Invoked;
    }

    [[nodiscard]] bool registerGeneratedBlocks(
        std::span<const PS2GeneratedBlockEntry> entries)
    {
        return blockDispatcher_.load(entries);
    }

    [[nodiscard]] BlockDispatchResult runGeneratedBlocks(std::size_t maxBlocks)
    {
        return blockDispatcher_.run(rdram_.data(), context_, &runtime_, maxBlocks);
    }

    [[nodiscard]] RecompiledBlockDispatcher &blockDispatcher() noexcept
    {
        return blockDispatcher_;
    }

    [[nodiscard]] std::span<std::uint8_t> eeRdram() noexcept { return rdram_; }
    [[nodiscard]] std::span<const std::uint8_t> eeRdram() const noexcept { return rdram_; }
    [[nodiscard]] R5900Context &context() noexcept { return context_; }
    [[nodiscard]] const R5900Context &context() const noexcept { return context_; }
    [[nodiscard]] PS2Runtime &runtime() noexcept { return runtime_; }
    [[nodiscard]] const PS2Runtime &runtime() const noexcept { return runtime_; }
    [[nodiscard]] iop::IomanDispatcher &ioman() noexcept { return ioman_; }
    [[nodiscard]] const iop::IomanTraceRing &trace() const noexcept { return trace_; }

    void reset() noexcept
    {
        ioman_.reset();
        context_ = {};
        std::fill(rdram_.begin(), rdram_.end(), std::uint8_t{0});
        trace_.clear();
        blockDispatcher_.clear();
    }

private:
    // runtime_ contains non-owning pointers to trace_ and ioman_; declaration
    // order ensures both dependencies outlive it.
    std::vector<std::uint8_t> rdram_;
    iop::IomanTraceRing trace_{};
    iop::IomanDispatcher ioman_;
    PS2Runtime runtime_{};
    R5900Context context_{};
    RecompiledBlockDispatcher blockDispatcher_{};
};
} // namespace ps2::xbox360
