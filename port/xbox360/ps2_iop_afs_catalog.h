#pragma once

#include <cstdint>
#include <optional>
#include <span>
#include <string>

namespace ps2::xbox360::iop
{
enum class AfsArchiveId : std::uint8_t { WAG, WAH, WAI, WAJ };
enum class AfsResourceType : std::uint8_t { Unknown, Texture, Model, Audio, Level, Animation, Other };
enum class AfsLoadStrategy : std::uint8_t { Cache, Stream, SeekAware, OnDemand };
struct AfsResourceKey { AfsArchiveId archive=AfsArchiveId::WAG; std::uint32_t entryIndex=0; friend constexpr bool operator==(AfsResourceKey,AfsResourceKey)=default; };

// Stable logical identifiers belong to the port, not to archive entry order.
// Do not add guessed archive indices here: populate the manifest only from an
// inspected retail inventory or a traced runtime request.
enum class AfsResourceId : std::uint32_t
{
    Invalid = 0
};

struct AfsResourceRecord
{
    AfsResourceId id = AfsResourceId::Invalid;
    AfsArchiveId archive = AfsArchiveId::WAG;
    std::uint32_t entryIndex = 0;
    AfsResourceType type = AfsResourceType::Unknown;
    std::uint64_t expectedSize = 0; // zero means not yet established
    AfsLoadStrategy strategy = AfsLoadStrategy::OnDemand;
};

[[nodiscard]] constexpr const char *afsArchiveFilename(AfsArchiveId archive) noexcept
{
    switch (archive)
    {
    case AfsArchiveId::WAG: return "GAMEDATA.WAG";
    case AfsArchiveId::WAH: return "GAMEDATA.WAH";
    case AfsArchiveId::WAI: return "GAMEDATA.WAI";
    case AfsArchiveId::WAJ: return "GAMEDATA.WAJ";
    }
    return nullptr;
}

class AfsResourceCatalog
{
public:
    explicit constexpr AfsResourceCatalog(std::span<const AfsResourceRecord> records) noexcept
        : records_(records) {}

    [[nodiscard]] const AfsResourceRecord *find(AfsResourceId id) const noexcept
    {
        if (id == AfsResourceId::Invalid) return nullptr;
        for (const auto &record : records_)
            if (record.id == id) return &record;
        return nullptr;
    }

    [[nodiscard]] const AfsResourceRecord *find(AfsArchiveId archive, std::uint32_t entryIndex) const noexcept\n    {\n        for (const auto &record : records_)\n            if (record.archive == archive && record.entryIndex == entryIndex) return &record;\n        return nullptr;\n    }\n\n    [[nodiscard]] bool valid() const noexcept\n    {\n        for (std::size_t i=0;i<records_.size();++i) {\n            if (records_[i].id == AfsResourceId::Invalid) return false;\n            for (std::size_t j=i+1;j<records_.size();++j)\n                if (records_[i].id == records_[j].id ||\n                    (records_[i].archive == records_[j].archive && records_[i].entryIndex == records_[j].entryIndex)) return false;\n        }\n        return true;\n    }\n\n    // Produces the existing streaming-IOMAN virtual path. It does not open the
    // file; callers pass this path through IomanDispatcher::Open with read-only flags.
    [[nodiscard]] std::optional<std::string> pathFor(AfsResourceId id) const
    {
        const auto *record = find(id);
        if (!record) return std::nullopt;
        const char *archive = afsArchiveFilename(record->archive);
        if (!archive) return std::nullopt;
        return std::string("afs0:/") + archive + "/" + std::to_string(record->entryIndex);
    }

    [[nodiscard]] std::span<const AfsResourceRecord> records() const noexcept { return records_; }

private:
    std::span<const AfsResourceRecord> records_;
};

// The verified retail mapping is intentionally empty until archive-entry
// contents are classified. This avoids silently treating arbitrary indices as assets.
inline constexpr std::span<const AfsResourceRecord> kVerifiedAfsResources{};

inline constexpr AfsResourceCatalog kVerifiedAfsResourceCatalog{kVerifiedAfsResources};
} // namespace ps2::xbox360::iop
