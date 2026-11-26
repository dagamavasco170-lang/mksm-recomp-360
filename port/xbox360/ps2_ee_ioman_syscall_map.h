#pragma once

#include "ps2_iop_ioman.h"

#include <array>
#include <cstddef>
#include <cstdint>

namespace ps2::xbox360::iop
{
// EE syscall IDs are game/build-specific and must be sourced from the actual
// executable or generated call sites. This table intentionally contains no
// guessed numeric syscall assignments.
struct EeIomanSyscallBinding
{
    std::uint32_t syscallId = 0;
    IomanCommand command = IomanCommand::Open;
};

template <std::size_t Capacity>
class EeIomanSyscallMap
{
public:
    [[nodiscard]] bool bind(std::uint32_t syscallId, IomanCommand command) noexcept
    {
        if (!isIomanCommand(command))
            return false;
        for (const auto &entry : entries_)
        {
            if (!entry.active)
                continue;
            if (entry.syscallId == syscallId)
                return entry.command == command;
        }
        for (auto &entry : entries_)
        {
            if (!entry.active)
            {
                entry = {syscallId, command, true};
                ++size_;
                return true;
            }
        }
        return false;
    }

    [[nodiscard]] bool lookup(std::uint32_t syscallId, IomanCommand &command) const noexcept
    {
        for (const auto &entry : entries_)
        {
            if (entry.active && entry.syscallId == syscallId)
            {
                command = entry.command;
                return true;
            }
        }
        return false;
    }

    [[nodiscard]] std::size_t size() const noexcept { return size_; }

private:
    struct Entry
    {
        std::uint32_t syscallId = 0;
        IomanCommand command = IomanCommand::Open;
        bool active = false;
    };

    [[nodiscard]] static constexpr bool isIomanCommand(IomanCommand command) noexcept
    {
        switch (command)
        {
        case IomanCommand::Open:
        case IomanCommand::Close:
        case IomanCommand::Read:
        case IomanCommand::Lseek:
            return true;
        default:
            return false;
        }
    }

    std::array<Entry, Capacity> entries_{};
    std::size_t size_ = 0;
};
} // namespace ps2::xbox360::iop
