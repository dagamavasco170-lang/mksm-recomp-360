#pragma once

#include "ps2_generated_runtime_abi.h"

#include <cstdint>
#include <span>

namespace ps2::xbox360::iop
{
// Adapter for the PS2 EE direct fio* ABI to the Xbox IOMAN dispatcher.
// It uses EE guest addresses and the runtime's bounded pointer translation.
// The dispatcher owns descriptor lifetime; this adapter does not duplicate files.
class EeFioIomanAdapter
{
public:
    static constexpr std::uint32_t FioReadOnly = 0x0001u;
    static constexpr std::uint32_t FioWriteOnly = 0x0002u;
    static constexpr std::uint32_t FioReadWrite = 0x0003u;
    static constexpr std::uint32_t FioAppend = 0x0100u;
    static constexpr std::uint32_t FioCreate = 0x0200u;
    static constexpr std::uint32_t FioTruncate = 0x0400u;
    static constexpr std::uint32_t FioExclusive = 0x0800u;

    explicit EeFioIomanAdapter(PS2Runtime &runtime) noexcept : runtime_(runtime) {}

    [[nodiscard]] std::int32_t open(std::uint32_t pathEeAddress,
                                  std::uint32_t ps2Flags,
                                  std::span<std::uint8_t> eeRdram) const
    {
        // Xbox IOMAN is currently read-only. Do not silently emulate create,
        // truncate, append, or write access with unsafe host-side behavior.
        if ((ps2Flags & 0x3u) != FioReadOnly ||
            (ps2Flags & (FioAppend | FioCreate | FioTruncate | FioExclusive)) != 0)
            return -EACCES;
        return runtime_.dispatchIomanEe(IomanCommand::Open, pathEeAddress,
                                       RetailFileIo::OpenReadOnly, 0, eeRdram);
    }

    [[nodiscard]] std::int32_t read(std::int32_t fd, std::uint32_t dstEeAddress,
                                   std::uint32_t size, std::span<std::uint8_t> eeRdram) const
    {
        return runtime_.dispatchIomanEe(IomanCommand::Read,
                                       static_cast<std::uint32_t>(fd),
                                       dstEeAddress, size, eeRdram);
    }

    [[nodiscard]] std::int32_t seek(std::int32_t fd, std::int32_t offset,
                                   std::uint32_t whence,
                                   std::span<std::uint8_t> eeRdram) const
    {
        if (whence > static_cast<std::uint32_t>(SeekWhence::End))
            return -EINVAL;
        return runtime_.dispatchIomanEe(IomanCommand::Lseek,
                                       static_cast<std::uint32_t>(fd),
                                       static_cast<std::uint32_t>(offset),
                                       whence, eeRdram);
    }

    [[nodiscard]] std::int32_t close(std::int32_t fd,
                                    std::span<std::uint8_t> eeRdram) const
    {
        return runtime_.dispatchIomanEe(IomanCommand::Close,
                                       static_cast<std::uint32_t>(fd),
                                       0, 0, eeRdram);
    }

private:
    PS2Runtime &runtime_;
};
} // namespace ps2::xbox360::iop
