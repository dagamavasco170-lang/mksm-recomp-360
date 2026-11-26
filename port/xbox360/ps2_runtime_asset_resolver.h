#pragma once
#include "ps2_resource_afs_catalog_manager.h"
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace ps2::xbox360::resources {
enum class AssetResolveStatus : std::uint8_t { Resolved, UnknownId, UnsupportedStrategy, LoadFailed, InvalidCatalog };
struct AssetResolveResult {
 AssetResolveStatus status=AssetResolveStatus::UnknownId;
 iop::AfsResourceId id=iop::AfsResourceId::Invalid;
 iop::AfsResourceKey key{};
 iop::AfsResourceType type=iop::AfsResourceType::Unknown;
 iop::AfsLoadStrategy strategy=iop::AfsLoadStrategy::OnDemand;
 const ResourceLoadingRuntime::Blob* cached=nullptr;
};
struct AssetDependency { iop::AfsResourceId parent=iop::AfsResourceId::Invalid, child=iop::AfsResourceId::Invalid; };
class RuntimeAssetResolver {
public:
 RuntimeAssetResolver(AfsCatalogResourceManager&manager,std::span<const AssetDependency>dependencies={})
  :manager_(manager),dependencies_(dependencies){}
 [[nodiscard]] AssetResolveResult resolve(iop::AfsResourceId id)const {
  AssetResolveResult r{};r.id=id;const auto*rec=manager_.resolve(id);
  if(!rec){r.status=AssetResolveStatus::UnknownId;return r;}
  r.status=AssetResolveStatus::Resolved;r.key={rec->archive,rec->entryIndex};r.type=rec->type;r.strategy=rec->strategy;r.expectedSize=rec->expectedSize;return r;
 }
 [[nodiscard]] AssetResolveResult resolve(iop::AfsArchiveId archive,std::uint32_t entry)const {
  const auto*rec=manager_.findByEntry(archive,entry);
  return rec?resolve(rec->id):AssetResolveResult{AssetResolveStatus::UnknownId,iop::AfsResourceId::Invalid,{archive,entry}};
 }
 [[nodiscard]] AssetResolveResult load(iop::AfsResourceId id,ResourceLoadingRuntime::Blob&transient) {
  transient.clear();auto r=resolve(id);if(r.status!=AssetResolveStatus::Resolved)return r;
  if(r.strategy==iop::AfsLoadStrategy::Cache){r.cached=manager_.loadCached(id);r.status=r.cached?AssetResolveStatus::Resolved:AssetResolveStatus::LoadFailed;}
  else if(r.strategy==iop::AfsLoadStrategy::OnDemand){r.status=manager_.loadOnDemand(id,transient)?AssetResolveStatus::Resolved:AssetResolveStatus::LoadFailed;}
  else r.status=AssetResolveStatus::UnsupportedStrategy;
  return r;
 }
 [[nodiscard]] std::int32_t read(iop::AfsResourceId id,std::uint64_t offset,std::span<std::uint8_t>out){return manager_.read(id,offset,out);}
 [[nodiscard]] std::vector<iop::AfsResourceId> dependencies(iop::AfsResourceId id)const {
  std::vector<iop::AfsResourceId> out;for(const auto&d:dependencies_)if(d.parent==id&&manager_.resolve(d.child))out.push_back(d.child);return out;
 }
 [[nodiscard]] bool dependencyGraphValid()const noexcept {
  for(const auto&d:dependencies_)if(d.parent==iop::AfsResourceId::Invalid||d.child==iop::AfsResourceId::Invalid||!manager_.resolve(d.parent)||!manager_.resolve(d.child)||d.parent==d.child)return false;
  for(std::size_t i=0;i<dependencies_.size();++i)for(std::size_t j=i+1;j<dependencies_.size();++j)
   if(dependencies_[i].parent==dependencies_[j].parent&&dependencies_[i].child==dependencies_[j].child)return false;
  return true;
 }
 [[nodiscard]] bool dependencyClosure(iop::AfsResourceId root,std::vector<iop::AfsResourceId>&out)const {
  out.clear();if(!manager_.resolve(root)||!dependencyGraphValid())return false;
  std::vector<iop::AfsResourceId> stack{root};
  while(!stack.empty()){auto id=stack.back();stack.pop_back();
   if(std::find(out.begin(),out.end(),id)!=out.end())continue;
   out.push_back(id);for(auto child:dependencies(id))stack.push_back(child);
  }return true;
 }
private:AfsCatalogResourceManager&manager_;std::span<const AssetDependency>dependencies_;
};
} // namespace ps2::xbox360::resources
