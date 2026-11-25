#pragma once

#include "ps2_retail_disc.h"

#include <algorithm>
#include <array>
#include <cerrno>
#include <cstdint>
#include <limits>
#include <span>
#include <string>
#include <string_view>
#include <utility>

namespace ps2::xbox360::iop
{
enum class SeekWhence : std::int32_t
{
    Set = 0,
    Current = 1,
    End = 2
};

// Read-only ioman-compatible file operations for an extracted retail disc.
// Guest addresses must be validated by the caller and exposed as a span;
// this class never treats a guest virtual address as a host pointer.
class RetailFileIo
{
public:
    static constexpr std::size_t MaxOpenFiles = 32;
    static constexpr std::int32_t FirstDescriptor = 3;

    explicit RetailFileIo(std::filesystem::path extractionRoot)
        : disc_(std::move(extractionRoot))
    {
    }

    // IOP O_RDONLY is zero. Writes and create/truncate flags are unsupported.
    [[nodiscard]] std::int32_t sceOpen(std::string_view path,
                                       std::int32_t flags = 0)
    {
        if (path.empty())
            return -EINVAL;
        if (flags != 0)
            return -EACCES;

        std::uint64_t size = 0;
        const auto status = disc_.fileSize(std::string(path), size);
        if (status != disc::FileStatus::Ok)
            return mapError(status);
        if (size > static_cast<std::uint64_t>(std::numeric_limits<std::int32_t>::max()))
            return -EOVERFLOW;

        for (std::size_t i = 0; i < files_.size(); ++i)
        {
            if (!files_[i].open)
            {
                files_[i].path.assign(path);
                files_[i].size = size;
                files_[i].position = 0;
                files_[i].open = true;
                return FirstDescriptor + static_cast<std::int32_t>(i);
            }
        }
        return -EMFILE;
    }

    // Returns bytes read, zero at EOF, or a negative IOP/POSIX errno value.
    [[nodiscard]] std::int32_t sceRead(std::int32_t descriptor,
                                       std::span<std::uint8_t> destination)
    {
        FileEntry *entry = find(descriptor);
        if (!entry)
            return -EBADF;
        if (destination.empty())
            return 0;
        if (entry->position >= entry->size)
            return 0;

        const std::uint64_t remaining = entry->size - entry->position;
        const std::size_t count = static_cast<std::size_t>(
            std::min<std::uint64_t>(remaining, destination.size()));
        const auto status = disc_.read(entry->path, entry->position,
                                       destination.first(count));
        if (status != disc::FileStatus::Ok)
            return mapError(status);

        entry->position += count;
        return static_cast<std::int32_t>(count);
    }

    // sceLseek-compatible SET/CUR/END semantics. Seeking past EOF is allowed.
    [[nodiscard]] std::int32_t sceLseek(std::int32_t descriptor,
                                        std::int32_t offset,
                                        SeekWhence whence)
    {
        FileEntry *entry = find(descriptor);
        if (!entry)
            return -EBADF;

        std::int64_t base = 0;
        switch (whence)
        {
        case SeekWhence::Set:
            base = 0;
            break;
        case SeekWhence::Current:
            base = static_cast<std::int64_t>(entry->position);
            break;
        case SeekWhence::End:
            base = static_cast<std::int64_t>(entry->size);
            break;
        default:
            return -EINVAL;
        }

        const std::int64_t next = base + static_cast<std::int64_t>(offset);
        if (next < 0 || next > std::numeric_limits<std::int32_t>::max())
            return -EINVAL;
        entry->position = static_cast<std::uint64_t>(next);
        return static_cast<std::int32_t>(next);
    }

    [[nodiscard]] std::int32_t sceClose(std::int32_t descriptor)
    {
        FileEntry *entry = find(descriptor);
        if (!entry)
            return -EBADF;
        *entry = {};
        return 0;
    }

    void reset() noexcept
    {
        for (auto &entry : files_)
            entry = {};
    }

    [[nodiscard]] std::size_t openFileCount() const noexcept
    {
        std::size_t count = 0;
        for (const auto &entry : files_)
            count += entry.open ? 1u : 0u;
        return count;
    }

private:
    struct FileEntry
    {
        std::string path;
        std::uint64_t size = 0;
        std::uint64_t position = 0;
        bool open = false;
    };

    [[nodiscard]] FileEntry *find(std::int32_t descriptor) noexcept
    {
        const std::int32_t index = descriptor - FirstDescriptor;
        if (index < 0 || static_cast<std::size_t>(index) >= files_.size())
            return nullptr;
        FileEntry &entry = files_[static_cast<std::size_t>(index)];
        return entry.open ? &entry : nullptr;
    }

    [[nodiscard]] static std::int32_t mapError(disc::FileStatus status) noexcept
    {
        switch (status)
        {
        case disc::FileStatus::Ok:
            return 0;
        case disc::FileStatus::NotFound:
            return -ENOENT;
        case disc::FileStatus::InvalidPath:
            return -EINVAL;
        case disc::FileStatus::OutOfRange:
            return -EINVAL;
        case disc::FileStatus::IoError:
        default:
            return -EIO;
        }
    }

    disc::RetailDisc disc_;
    std::array<FileEntry, MaxOpenFiles> files_{};
};
} // namespace ps2::xbox360::iop
