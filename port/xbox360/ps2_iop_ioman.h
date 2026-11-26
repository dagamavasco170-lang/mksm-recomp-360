#pragma once

#include "ps2_iop_retail_fileio.h"
#include "ps2_iop_afs_stream.h"
#include "ps2_iop_afs_catalog.h"

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
struct IomanTraceRecord
{
    IomanCommand command = IomanCommand::Open;
    std::string_view path; // only populated for Open; never contains file data
    std::int32_t descriptor = -1;
    std::uint32_t arg1 = 0;
    std::uint32_t arg2 = 0;
    std::int32_t result = 0;
};

class IomanTraceSink
{
public:
    virtual ~IomanTraceSink() = default;
    virtual void onIomanTrace(const IomanTraceRecord &record) noexcept = 0;
};

// Optional metadata-only tracing seam. When no sink is attached, dispatch has no
// trace callback overhead beyond a predictable null check. The sink owns storage
// and may bound/dump records according to the target platform.
class IomanDispatcher
{
public:
    static constexpr std::size_t MaxPathBytes = 4096u;

    explicit IomanDispatcher(std::filesystem::path extractionRoot,
                            IomanTraceSink *traceSink = nullptr)
        : files_(extractionRoot), afsFiles_(std::move(extractionRoot)),
          traceSink_(traceSink)
    {
    }

    void setTraceSink(IomanTraceSink *sink) noexcept { traceSink_ = sink; }

    [[nodiscard]] std::int32_t dispatch(IomanCommand command,
                                        std::uint32_t a0,
                                        std::uint32_t a1,
                                        std::uint32_t a2,
                                        std::span<std::uint8_t> iopRam)
    {
        std::string tracePath;
        std::int32_t result = -ENOSYS;
        switch (command)
        {
        case IomanCommand::Open:
        {
            std::string path;
            if (!readGuestString(iopRam, a0, path))
                result = -EFAULT;
            else
            {
                tracePath = path;
                if (path.starts_with("afs0:/"))
                    result = a1 == RetailFileIo::OpenReadOnly ? afsFiles_.open(path) : -EACCES;
                else
                    result = files_.sceOpen(path, static_cast<std::int32_t>(a1));
            }
            break;
        }
        case IomanCommand::Close:
        {
            const auto fd = static_cast<std::int32_t>(a0);
            result = afsFiles_.ownsDescriptor(fd) ? afsFiles_.close(fd) : files_.sceClose(fd);
            break;
        }
        case IomanCommand::Read:
        {
            if (a1 > iopRam.size() || a2 > iopRam.size() - a1)
                result = -EFAULT;
            else
            {
                auto destination = iopRam.subspan(a1, a2);
                const auto fd = static_cast<std::int32_t>(a0);
                result = afsFiles_.ownsDescriptor(fd) ? afsFiles_.read(fd, destination)
                                                     : files_.sceRead(fd, destination);
            }
            break;
        }
        case IomanCommand::Lseek:
            if (a2 > static_cast<std::uint32_t>(SeekWhence::End))
                result = -EINVAL;
            else
            {
                const auto fd = static_cast<std::int32_t>(a0);
                result = afsFiles_.ownsDescriptor(fd)
                    ? afsFiles_.seek(fd, static_cast<std::int32_t>(a1), static_cast<SeekWhence>(a2))
                    : files_.sceLseek(fd, static_cast<std::int32_t>(a1), static_cast<SeekWhence>(a2));
            }
            break;
        default:
            result = -ENOSYS;
            break;
        }
        emitTrace(command, tracePath, a0, a1, a2, result);
        return result;
    }

    [[nodiscard]] std::int32_t openAfsResource(const AfsResourceCatalog &catalog,
                                                   AfsResourceId resourceId)
    {
        const auto path = catalog.pathFor(resourceId);
        const std::string tracePath = path.value_or(std::string{});
        const auto result = path ? afsFiles_.open(*path) : -ENOENT;
        emitTrace(IomanCommand::Open, tracePath, 0u, RetailFileIo::OpenReadOnly, 0u, result);
        return result;
    }

    void reset() noexcept { files_.reset(); afsFiles_.reset(); }
    [[nodiscard]] std::size_t openFileCount() const noexcept
    {
        return files_.openFileCount();
    }

private:
    void emitTrace(IomanCommand command, std::string_view path,
                  std::uint32_t a0, std::uint32_t a1, std::uint32_t a2,
                  std::int32_t result) noexcept
    {
        if (traceSink_)
        {
            traceSink_->onIomanTrace({command, path,
                command == IomanCommand::Open ? result : static_cast<std::int32_t>(a0), a1, a2, result});
        }
    }

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
    AfsStreamIo afsFiles_;
    IomanTraceSink *traceSink_ = nullptr;
};
} // namespace ps2::xbox360::iop
