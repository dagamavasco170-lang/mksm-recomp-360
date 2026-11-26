#pragma once

#include "ps2_iop_retail_fileio.h"
#include "ps2_iop_afs_stream.h"
#include "ps2_iop_afs_catalog.h"
#include "ps2_asset_service.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <array>
#include <limits>

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
    static constexpr std::size_t MaxAssetServiceFiles = 16u;
    static constexpr std::int32_t AssetServiceFirstDescriptor = 2000;

    explicit IomanDispatcher(std::filesystem::path extractionRoot,
                            IomanTraceSink *traceSink = nullptr)
        : files_(extractionRoot), afsFiles_(std::move(extractionRoot)),
          traceSink_(traceSink)
    {
    }

    void setAssetService(resources::AssetService *service) noexcept { assetService_ = service; }
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
                {
                    if (a1 != RetailFileIo::OpenReadOnly)
                        result = -EACCES;
                    else
                    {
                        result = openCatalogAsset(path);
                        if (result == -ENOENT || result == -ENOTSUP)
                            result = afsFiles_.open(path);
                    }
                }
                else
                    result = files_.sceOpen(path, static_cast<std::int32_t>(a1));
            }
            break;
        }
        case IomanCommand::Close:
        {
            const auto fd = static_cast<std::int32_t>(a0);
            if (ownsAssetDescriptor(fd))
                result = closeAssetDescriptor(fd);
            else
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
                if (ownsAssetDescriptor(fd))
                    result = readAssetDescriptor(fd, destination);
                else
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
                if (ownsAssetDescriptor(fd))
                    result = seekAssetDescriptor(fd, static_cast<std::int32_t>(a1), static_cast<SeekWhence>(a2));
                else
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

    void reset() noexcept { releaseAssetDescriptors(); files_.reset(); afsFiles_.reset(); }
    [[nodiscard]] std::size_t openFileCount() const noexcept
    {
        return files_.openFileCount();
    }

private:
    struct AssetFile
    {
        resources::AssetHandle handle{};
        std::uint64_t size = 0;
        std::uint64_t position = 0;
        bool open = false;
    };

    [[nodiscard]] bool ownsAssetDescriptor(std::int32_t fd) const noexcept
    {
        const auto index = fd - AssetServiceFirstDescriptor;
        return index >= 0 && static_cast<std::size_t>(index) < assetFiles_.size() &&
               assetFiles_[static_cast<std::size_t>(index)].open;
    }

    [[nodiscard]] std::int32_t openCatalogAsset(std::string_view path)
    {
        if (!assetService_)
            return -ENOTSUP;
        constexpr std::string_view prefix = "afs0:/";
        if (!path.starts_with(prefix))
            return -ENOENT;
        path.remove_prefix(prefix.size());
        const auto slash = path.find('/');
        if (slash == std::string_view::npos || slash == 0 || slash + 1 >= path.size())
            return -ENOENT;
        const auto archiveName = path.substr(0, slash);
        const auto indexText = path.substr(slash + 1);
        AfsArchiveId archive;
        if (archiveName == "GAMEDATA.WAG") archive = AfsArchiveId::WAG;
        else if (archiveName == "GAMEDATA.WAH") archive = AfsArchiveId::WAH;
        else if (archiveName == "GAMEDATA.WAI") archive = AfsArchiveId::WAI;
        else if (archiveName == "GAMEDATA.WAJ") archive = AfsArchiveId::WAJ;
        else return -ENOENT;
        std::uint32_t entry = 0;
        for (const char ch : indexText)
        {
            if (ch < '0' || ch > '9')
                return -ENOENT;
            const auto digit = static_cast<std::uint32_t>(ch - '0');
            if (entry > (std::numeric_limits<std::uint32_t>::max() - digit) / 10u)
                return -ENOENT;
            entry = entry * 10u + digit;
        }
        const auto acquired = assetService_->acquireEntry(archive, entry);
        if (acquired.status != resources::AssetServiceStatus::Ok)
            return -ENOENT;
        const auto size = assetService_->size(acquired.handle);
        if (size == 0)
        {
            (void)assetService_->release(acquired.handle);
            return -ENOTSUP;
        }
        for (std::size_t slot = 0; slot < assetFiles_.size(); ++slot)
        {
            if (!assetFiles_[slot].open)
            {
                assetFiles_[slot] = {acquired.handle, size, 0, true};
                return AssetServiceFirstDescriptor + static_cast<std::int32_t>(slot);
            }
        }
        (void)assetService_->release(acquired.handle);
        return -EMFILE;
    }

    [[nodiscard]] std::int32_t readAssetDescriptor(std::int32_t fd, std::span<std::uint8_t> out)
    {
        auto &file = assetFiles_[static_cast<std::size_t>(fd - AssetServiceFirstDescriptor)];
        if (out.empty() || file.position >= file.size)
            return 0;
        const auto count = static_cast<std::size_t>(std::min<std::uint64_t>(file.size - file.position, out.size()));
        const auto read = assetService_->read(file.handle, file.position, out.first(count));
        if (read < 0)
            return -EIO;
        file.position += static_cast<std::uint64_t>(read);
        return read;
    }

    [[nodiscard]] std::int32_t seekAssetDescriptor(std::int32_t fd, std::int32_t offset, SeekWhence whence)
    {
        auto &file = assetFiles_[static_cast<std::size_t>(fd - AssetServiceFirstDescriptor)];
        std::int64_t base = 0;
        switch (whence)
        {
        case SeekWhence::Set: base = 0; break;
        case SeekWhence::Current: base = static_cast<std::int64_t>(file.position); break;
        case SeekWhence::End: base = static_cast<std::int64_t>(file.size); break;
        default: return -EINVAL;
        }
        const auto next = base + offset;
        if (next < 0 || next > std::numeric_limits<std::int32_t>::max())
            return -EINVAL;
        file.position = static_cast<std::uint64_t>(next);
        return static_cast<std::int32_t>(next);
    }

    [[nodiscard]] std::int32_t closeAssetDescriptor(std::int32_t fd)
    {
        auto &file = assetFiles_[static_cast<std::size_t>(fd - AssetServiceFirstDescriptor)];
        const bool released = assetService_->release(file.handle);
        file = {};
        return released ? 0 : -EBADF;
    }

    void releaseAssetDescriptors() noexcept
    {
        if (assetService_)
            for (auto &file : assetFiles_)
                if (file.open)
                    (void)assetService_->release(file.handle);
        for (auto &file : assetFiles_)
            file = {};
    }

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
    resources::AssetService *assetService_ = nullptr;
    std::array<AssetFile, MaxAssetServiceFiles> assetFiles_{};
    IomanTraceSink *traceSink_ = nullptr;
};
} // namespace ps2::xbox360::iop
