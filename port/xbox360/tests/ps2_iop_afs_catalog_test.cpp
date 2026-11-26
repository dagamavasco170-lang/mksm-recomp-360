#include "ps2_iop_afs_catalog.h"

#include <array>
#include <cassert>
#include <string>

int main()
{
    using namespace ps2::xbox360::iop;
    constexpr std::array records{
        AfsResourceRecord{static_cast<AfsResourceId>(1), AfsArchiveId::WAG, 7,
                          AfsResourceType::Texture, 4096},
        AfsResourceRecord{static_cast<AfsResourceId>(2), AfsArchiveId::WAI, 19,
                          AfsResourceType::Audio, 0},
        AfsResourceRecord{static_cast<AfsResourceId>(3), AfsArchiveId::WAJ, 0,
                          AfsResourceType::Level, 65536},
        AfsResourceRecord{static_cast<AfsResourceId>(4), AfsArchiveId::WAH, 2,
                          AfsResourceType::Model, 8192},
    };
    const AfsResourceCatalog catalog{records};
    assert(catalog.records().size() == 4);
    assert(catalog.find(static_cast<AfsResourceId>(1)) != nullptr);
    assert(catalog.find(static_cast<AfsResourceId>(1))->expectedSize == 4096);
    assert(catalog.find(AfsResourceId::Invalid) == nullptr);
    assert(catalog.find(static_cast<AfsResourceId>(99)) == nullptr);
    assert(catalog.pathFor(static_cast<AfsResourceId>(1)) ==
           std::optional<std::string>{"afs0:/GAMEDATA.WAG/7"});
    assert(catalog.pathFor(static_cast<AfsResourceId>(2)) ==
           std::optional<std::string>{"afs0:/GAMEDATA.WAI/19"});
    assert(catalog.pathFor(static_cast<AfsResourceId>(3)) ==
           std::optional<std::string>{"afs0:/GAMEDATA.WAJ/0"});
    assert(catalog.pathFor(static_cast<AfsResourceId>(4)) ==
           std::optional<std::string>{"afs0:/GAMEDATA.WAH/2"});
    assert(!catalog.pathFor(static_cast<AfsResourceId>(99)).has_value());
    assert(std::string(afsArchiveFilename(AfsArchiveId::WAG)) == "GAMEDATA.WAG");
    assert(std::string(afsArchiveFilename(AfsArchiveId::WAH)) == "GAMEDATA.WAH");
    assert(std::string(afsArchiveFilename(AfsArchiveId::WAI)) == "GAMEDATA.WAI");
    assert(std::string(afsArchiveFilename(AfsArchiveId::WAJ)) == "GAMEDATA.WAJ");
    assert(kVerifiedAfsResourceCatalog.records().empty());
    return 0;
}
