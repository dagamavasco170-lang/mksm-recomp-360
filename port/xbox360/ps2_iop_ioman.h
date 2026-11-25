#pragma once

#include "ps2_iop_retail_fileio.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>

namespace ps2::xbox360::iop
{
enum class IomanCommand : std::uint32_t
{
    Open = 1,
    Close = 2,
    Read = 3,
    Lseek = 4
};

// Thin guest-memory boundary for the IOP ioman file calls. Arguments mirror
// the syscall register payloads: open(path, flags), read(fd, dst, size),
// lseek(fd, offset, whence), close(fd). Guest addresses are offsets into the
// supplied IOP RAM span, never host pointers.
class IomanDispatcher
{
public:
    static constexpr std::size_t MaxPathBytes = 4096u;

    explicit IomanDispatcher(std::filesystem::path extractionRoot)
        : files_(std::move(extractionRoot))
    {
    }

    [[nodiscard]] std::int32_t dispatch(IomanCommand command,
                                        std::uint32_t a0,
                                        std::uint32_t a1,
                                        std::uint32_t a2,
                                        std::span<std::uint8_t> iopRam)
    {
        switch (command)
        {
        case IomanCommand::Open:
        {
            std::string path;
            if (!readGuestString(iopRam, a0, path))
                return -EFAULT;
            return files_.sceOpen(path, static_cast<std::int32_t>(a1));
        }
        case IomanCommand::Close:
            return files_.sceClose(static_cast<std::int32_t>(a0));
        case IomanCommand::Read:
        {
            if (a1 > iopRam.size() || a2 > iopRam.size() - a1)
                return -EFAULT;
            auto destination = iopRam.subspan(a1, a2);
            return files_.sceRead(static_cast<std::int32_t>(a0), destination);
        }
        case IomanCommand::Lseek:
            if (a2 > static_cast<std::uint32_t>(SeekWhence::End))
                return -EINVAL;
            return files_.sceLseek(static_cast<std::int32_t>(a0),
                                   static_cast<std::int32_t>(a1),
                                   static_cast<SeekWhence>(a2));
        default:
            return -ENOSYS;
        }
    }

    void reset() noexcept { files_.reset(); }
    [[nodiscard]] std::size_t openFileCount() const noexcept
    {
        return files_.openFileCount();
    }

private:
    static bool readGuestString(std::span<const std::uint8_t> ram,
                                std::uint32_t address,
                                std::string &out)
    {
        if (address >= ram.size())
            return false;
        const std::size_t available = ram.size() - address;
        const std::size_t limit = std::min(available, MaxPathBytes);
        std::size_t length = 0;
        while (length < limit && ram[address + length] != 0u)
            ++length;
        if (length == limit)
            return false;
        out.assign(reinterpret_cast<const char *>(ram.data() + address), length);
        return !out.empty();
    }

    RetailFileIo files_;
};
} // namespace ps2::xbox360::iop
