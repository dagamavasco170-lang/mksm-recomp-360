#pragma once
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>

namespace ps2::xbox360::iop {
enum class AfsArchiveId : std::uint8_t { WAG, WAH, WAI, WAJ };
enum class AfsResourceType : std::uint8_t { Unknown, Texture, Model, Audio, Level, Animation, Other };
enum class AfsLoadStrategy : std::uint8_t { Cache, Stream, SeekAware, OnDemand };
struct AfsResourceKey {
    AfsArchiveId archive=AfsArchiveId::WAG;
    std::uint32_t entryIndex=0;
    friend constexpr bool operator==(AfsResourceKey,AfsResourceKey)=default;
};
enum class AfsResourceId : std::uint32_t { Invalid = 0 };
struct AfsResourceRecord {
    AfsResourceId id=AfsResourceId::Invalid;
    AfsArchiveId archive=AfsArchiveId::WAG;
    std::uint32_t entryIndex=0;
    AfsResourceType type=AfsResourceType::Unknown;
    std::uint64_t expectedSize=0;
    AfsLoadStrategy strategy=AfsLoadStrategy::OnDemand;
};
[[nodiscard]] constexpr const char *afsArchiveFilename(AfsArchiveId archive) noexcept {
    switch(archive) {
    case AfsArchiveId::WAG:return "GAMEDATA.WAG";
    case AfsArchiveId::WAH:return "GAMEDATA.WAH";
    case AfsArchiveId::WAI:return "GAMEDATA.WAI";
    case AfsArchiveId::WAJ:return "GAMEDATA.WAJ";
    } return nullptr;
}
class AfsResourceCatalog {
public:
    explicit constexpr AfsResourceCatalog(std::span<const AfsResourceRecord> records) noexcept:records_(records){}
    [[nodiscard]] const AfsResourceRecord* find(AfsResourceId id)const noexcept {
        if(id==AfsResourceId::Invalid)return nullptr;
        for(const auto&r:records_)if(r.id==id)return &r;return nullptr;
    }
    [[nodiscard]] const AfsResourceRecord* find(AfsArchiveId archive,std::uint32_t entry)const noexcept {
        for(const auto&r:records_)if(r.archive==archive&&r.entryIndex==entry)return &r;return nullptr;
    }
    [[nodiscard]] bool valid()const noexcept {
        for(std::size_t i=0;i<records_.size();++i) {
            if(records_[i].id==AfsResourceId::Invalid)return false;
            for(std::size_t j=i+1;j<records_.size();++j)
                if(records_[i].id==records_[j].id ||
                   (records_[i].archive==records_[j].archive&&records_[i].entryIndex==records_[j].entryIndex))return false;
        } return true;
    }
    [[nodiscard]] std::optional<std::string> pathFor(AfsResourceId id)const {
        if(!valid())return std::nullopt;
        const auto*r=find(id);if(!r)return std::nullopt;
        const auto*archive=afsArchiveFilename(r->archive);if(!archive)return std::nullopt;
        return std::string("afs0:/")+archive+"/"+std::to_string(r->entryIndex);
    }
    [[nodiscard]] std::span<const AfsResourceRecord> records()const noexcept{return records_;}
private:std::span<const AfsResourceRecord> records_;
};
// Empty until verified archive-entry metadata exists; never fill with guessed indices.
inline constexpr std::span<const AfsResourceRecord> kVerifiedAfsResources{};
inline constexpr AfsResourceCatalog kVerifiedAfsResourceCatalog{kVerifiedAfsResources};
} // namespace ps2::xbox360::iop
