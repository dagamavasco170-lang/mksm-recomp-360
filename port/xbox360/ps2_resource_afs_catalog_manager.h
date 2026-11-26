#pragma once
#include "ps2_iop_afs_catalog.h"
#include "ps2_resource_afs_bridge.h"
#include <cstdint>
#include <span>

namespace ps2::xbox360::resources {
// Logical-ID facade. A catalog record selects archive, entry and load policy;
// the catalog itself remains data-driven and retail entries stay unverified.
class AfsCatalogResourceManager {
public:
 AfsCatalogResourceManager(iop::AfsResourceCatalog const&catalog,
   iop::AfsStreamIo&wag,iop::AfsStreamIo&wah,iop::AfsStreamIo&wai,iop::AfsStreamIo&waj,
   ResourceLoadingRuntime&runtime):catalog_(catalog),wag_(wag),wah_(wah),wai_(wai),waj_(waj),runtime_(runtime){}
 [[nodiscard]] const iop::AfsResourceRecord* resolve(iop::AfsResourceId id)const noexcept{return catalog_.valid()?catalog_.find(id):nullptr;}
 [[nodiscard]] const ResourceLoadingRuntime::Blob* loadCached(iop::AfsResourceId id){
  const auto*r=resolve(id);if(!r||r->strategy!=iop::AfsLoadStrategy::Cache)return nullptr;
  auto&bridge=bridgeFor(r->archive);return runtime_.loadCached(static_cast<ResourceLoadingRuntime::ResourceId>(id),bridge.loader(r->entryIndex));
 }
 [[nodiscard]] bool loadOnDemand(iop::AfsResourceId id,ResourceLoadingRuntime::Blob&out){
  const auto*r=resolve(id);if(!r||r->strategy!=iop::AfsLoadStrategy::OnDemand)return false;
  return bridgeFor(r->archive).loadBlob(r->entryIndex,out);
 }
 [[nodiscard]] std::int32_t read(iop::AfsResourceId id,std::uint64_t offset,std::span<std::uint8_t>out){
  const auto*r=resolve(id);if(!r||(r->strategy!=iop::AfsLoadStrategy::Stream&&r->strategy!=iop::AfsLoadStrategy::SeekAware))return -1;
  return runtime_.stream(static_cast<ResourceLoadingRuntime::ResourceId>(id),offset,out,bridgeFor(r->archive).streamReader(r->entryIndex));
 }
 [[nodiscard]] const iop::AfsResourceRecord* findByEntry(iop::AfsArchiveId archive,std::uint32_t entry)const noexcept{return catalog_.valid()?catalog_.find(archive,entry):nullptr;}
private:
 AfsResourceBridge&bridgeFor(iop::AfsArchiveId archive){
  switch(archive){case iop::AfsArchiveId::WAG:return wagBridge_;case iop::AfsArchiveId::WAH:return wahBridge_;case iop::AfsArchiveId::WAI:return waiBridge_;case iop::AfsArchiveId::WAJ:return wajBridge_;}
  return wagBridge_;
 }
 // Bridges use the corresponding AFS reader and canonical archive filename.
 AfsResourceBridge wagBridge_{wag_,"GAMEDATA.WAG"},wahBridge_{wah_,"GAMEDATA.WAH"},waiBridge_{wai_,"GAMEDATA.WAI"},wajBridge_{waj_,"GAMEDATA.WAJ"};
 iop::AfsResourceCatalog const&catalog_;iop::AfsStreamIo&wag_;iop::AfsStreamIo&wah_;iop::AfsStreamIo&wai_;iop::AfsStreamIo&waj_;ResourceLoadingRuntime&runtime_;
};
} // namespace ps2::xbox360::resources
