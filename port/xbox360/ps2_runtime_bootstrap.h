#pragma once

#include "ps2_generated_runtime_abi.h"
#include "ps2_iop_ioman_trace_ring.h"

#include <cstdint>
#include <filesystem>
#include <span>

namespace ps2::xbox360
{
// Owns the first end-to-end EE -> generated ABI -> IOMAN -> host-file
// composition. EE RDRAM remains caller-owned and syscall numbers remain
// explicitly supplied from verified title metadata.
class PS2RuntimeBootstrap
{
public:
    explicit PS2RuntimeBootstrap(const std::filesystem::path &discRoot)
        : ioman_(discRoot)
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
        std::uint32_t a2, std::span<std::uint8_t> eeRdram)
    {
        return runtime_.dispatchEeIomanSyscall(syscallId, a0, a1, a2, eeRdram);
    }

    [[nodiscard]] PS2Runtime &runtime() noexcept { return runtime_; }
    [[nodiscard]] const PS2Runtime &runtime() const noexcept { return runtime_; }
    [[nodiscard]] iop::IomanDispatcher &ioman() noexcept { return ioman_; }
    [[nodiscard]] const iop::IomanTraceRing &trace() const noexcept { return trace_; }

    void reset() noexcept { ioman_.reset(); }

private:
    // Declaration order is lifetime order: runtime's non-owning pointers refer
    // to the trace and dispatcher, which outlive it during destruction.
    iop::IomanTraceRing trace_{};
    iop::IomanDispatcher ioman_;
    PS2Runtime runtime_{};
};
} // namespace ps2::xbox360
