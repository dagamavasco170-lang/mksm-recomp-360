#pragma once

#include "ps2_iop_retail_fileio.h"\n\n#include <algorithm>

#include <array>
#include <cerrno>
#include <cstdint>
#include <filesystem>
#include <limits>
#include <string>
#include <string_view>
#include <vector>

namespace ps2::xbox360::iop
{
// Streaming AFS entry reader. It reads only the archive header/table and requested
// payload ranges; large archive files are never loaded into RAM in their entirety.
class AfsStreamIo
{
public:
    static constexpr std::size_t MaxOpenFiles = 16;
    static constexpr std::int32_t FirstDescriptor = 1000;
    static constexpr std::uint32_t MaxEntries = 1'000'000;
    explicit AfsStreamIo(std::filesystem::path root) : disc_(std::move(root)) {}

    // Virtual path syntax: afs0:/GAMEDATA.WAG/<entry-index>
    [[nodiscard]] std::int32_t open(std::string_view virtualPath)
    {
        constexpr std::string_view prefix = "afs0:/";
        if (!virtualPath.starts_with(prefix))
            return -EINVAL;
        virtualPath.remove_prefix(prefix.size());
        const auto slash = virtualPath.find('/');
        if (slash == std::string_view::npos || slash == 0 || slash + 1 >= virtualPath.size())
            return -EINVAL;
        const std::string archive(virtualPath.substr(0, slash));
        const std::string indexText(virtualPath.substr(slash + 1));
        std::uint32_t index = 0;
        if (indexText.empty())
            return -EINVAL;
        for (char c : indexText)
        {
            if (c < '0' || c > '9')
                return -EINVAL;
            const std::uint32_t digit = static_cast<std::uint32_t>(c - '0');
            if (index > (std::numeric_limits<std::uint32_t>::max() - digit) / 10u)
                return -EINVAL;
            index = index * 10u + digit;
        }

        std::array<std::uint8_t, 8> header{};
        if (disc_.read(archive, 0, header) != disc::FileStatus::Ok)
            return -ENOENT;
        if (header[0] != 'A' || header[1] != 'F' || header[2] != 'S' || header[3] != 0)
            return -EINVAL;
        const std::uint32_t count = readLe32(header.data() + 4);
        if (count > MaxEntries || index >= count)
            return -EINVAL;
        const std::uint64_t tableEnd = 8ull + static_cast<std::uint64_t>(count) * 8ull;
        std::uint64_t archiveSize = 0;
        if (disc_.fileSize(archive, archiveSize) != disc::FileStatus::Ok || tableEnd > archiveSize)
            return -EIO;
        std::array<std::uint8_t, 8> record{};
        const std::uint64_t recordOffset = 8ull + static_cast<std::uint64_t>(index) * 8ull;
        if (disc_.read(archive, recordOffset, record) != disc::FileStatus::Ok)
            return -EIO;
        const std::uint32_t offset = readLe32(record.data());
        const std::uint32_t size = readLe32(record.data() + 4);
        if (offset < tableEnd || offset > archiveSize || size > archiveSize - offset)
            return -EINVAL;

        for (std::size_t slot = 0; slot < files_.size(); ++slot)
        {
            if (!files_[slot].open)
            {
                files_[slot] = {archive, offset, size, 0, true};
                return FirstDescriptor + static_cast<std::int32_t>(slot);
            }
        }
        return -EMFILE;
    }

    [[nodiscard]] std::int32_t read(std::int32_t fd, std::span<std::uint8_t> output)
    {
        File *file = find(fd);
        if (!file)
            return -EBADF;
        if (output.empty() || file->position >= file->size)
            return 0;
        const auto count = static_cast<std::size_t>(
            std::min<std::uint64_t>(file->size - file->position, output.size()));
        const auto status = disc_.read(file->archive, file->offset + file->position, output.first(count));
        if (status != disc::FileStatus::Ok)
            return status == disc::FileStatus::OutOfRange ? -EINVAL : -EIO;
        file->position += count;
        return static_cast<std::int32_t>(count);
    }

    [[nodiscard]] std::int32_t seek(std::int32_t fd, std::int32_t offset, SeekWhence whence)
    {
        File *file = find(fd);
        if (!file)
            return -EBADF;
        std::int64_t base = 0;
        switch (whence)
        {
        case SeekWhence::Set: base = 0; break;
        case SeekWhence::Current: base = static_cast<std::int64_t>(file->position); break;
        case SeekWhence::End: base = static_cast<std::int64_t>(file->size); break;
        default: return -EINVAL;
        }
        const std::int64_t next = base + offset;
        if (next < 0 || next > std::numeric_limits<std::int32_t>::max())
            return -EINVAL;
        file->position = static_cast<std::uint64_t>(next);
        return static_cast<std::int32_t>(next);
    }

    [[nodiscard]] std::int32_t close(std::int32_t fd)
    {
        File *file = find(fd);
        if (!file)
            return -EBADF;
        *file = {};
        return 0;
    }

    void reset() noexcept { for (auto &file : files_) file = {}; }
    [[nodiscard]] bool ownsDescriptor(std::int32_t fd) const noexcept
    {
        const auto index = fd - FirstDescriptor;
        return index >= 0 && static_cast<std::size_t>(index) < files_.size() &&
               files_[static_cast<std::size_t>(index)].open;
    }

private:
    struct File
    {
        std::string archive;
        std::uint64_t offset = 0;
        std::uint64_t size = 0;
        std::uint64_t position = 0;
        bool open = false;
    };
    static std::uint32_t readLe32(const std::uint8_t *p) noexcept
    {
        return static_cast<std::uint32_t>(p[0]) |
               (static_cast<std::uint32_t>(p[1]) << 8) |
               (static_cast<std::uint32_t>(p[2]) << 16) |
               (static_cast<std::uint32_t>(p[3]) << 24);
    }
    File *find(std::int32_t fd) noexcept
    {
        const auto index = fd - FirstDescriptor;
        if (index < 0 || static_cast<std::size_t>(index) >= files_.size())
            return nullptr;
        File &file = files_[static_cast<std::size_t>(index)];
        return file.open ? &file : nullptr;
    }
    disc::RetailDisc disc_;
    std::array<File, MaxOpenFiles> files_{};
};
} // namespace ps2::xbox360::iop
