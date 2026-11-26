#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace ps2::xbox360::disc
{
enum class AfsStatus
{
    Ok,
    BadMagic,
    TruncatedHeader,
    CountTooLarge,
    TruncatedTable,
    InvalidEntry
};

struct AfsEntry
{
    std::uint32_t offset = 0;
    std::uint32_t size = 0;
};

// Read-only view of the PS2 AFS container format used by MKSM data archives.
// No game data is copied or embedded. The backing bytes must outlive this view.
class AfsArchive
{
public:
    static constexpr std::size_t HeaderSize = 8;
    static constexpr std::size_t EntrySize = 8;
    static constexpr std::uint32_t MaxEntries = 1'000'000;

    [[nodiscard]] AfsStatus open(std::span<const std::uint8_t> bytes)
    {
        entries_.clear();
        bytes_ = {};
        if (bytes.size() < HeaderSize)
            return AfsStatus::TruncatedHeader;
        if (bytes[0] != 'A' || bytes[1] != 'F' || bytes[2] != 'S' || bytes[3] != 0)
            return AfsStatus::BadMagic;

        const std::uint32_t count = readLe32(bytes.data() + 4);
        if (count > MaxEntries)
            return AfsStatus::CountTooLarge;

        const std::size_t tableBytes = static_cast<std::size_t>(count) * EntrySize;
        if (tableBytes > bytes.size() - HeaderSize)
            return AfsStatus::TruncatedTable;
        const std::size_t tableEnd = HeaderSize + tableBytes;

        std::vector<AfsEntry> candidate;
        candidate.reserve(count);
        for (std::uint32_t i = 0; i < count; ++i)
        {
            const auto *record = bytes.data() + HeaderSize + static_cast<std::size_t>(i) * EntrySize;
            const AfsEntry entry{readLe32(record), readLe32(record + 4)};
            if (entry.offset < tableEnd || entry.offset > bytes.size() ||
                entry.size > bytes.size() - entry.offset)
                return AfsStatus::InvalidEntry;
            candidate.push_back(entry);
        }

        bytes_ = bytes;
        entries_ = std::move(candidate);
        return AfsStatus::Ok;
    }

    [[nodiscard]] std::size_t entryCount() const noexcept { return entries_.size(); }
    [[nodiscard]] bool isOpen() const noexcept { return !bytes_.empty(); }

    [[nodiscard]] const AfsEntry *metadata(std::size_t index) const noexcept
    {
        return index < entries_.size() ? &entries_[index] : nullptr;
    }

    [[nodiscard]] std::span<const std::uint8_t> entry(std::size_t index) const noexcept
    {
        const AfsEntry *item = metadata(index);
        if (!item || bytes_.empty())
            return {};
        return bytes_.subspan(item->offset, item->size);
    }

private:
    static std::uint32_t readLe32(const std::uint8_t *p) noexcept
    {
        return static_cast<std::uint32_t>(p[0]) |
               (static_cast<std::uint32_t>(p[1]) << 8) |
               (static_cast<std::uint32_t>(p[2]) << 16) |
               (static_cast<std::uint32_t>(p[3]) << 24);
    }

    std::span<const std::uint8_t> bytes_{};
    std::vector<AfsEntry> entries_{};
};
} // namespace ps2::xbox360::disc
