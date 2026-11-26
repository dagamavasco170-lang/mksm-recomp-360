#pragma once
#include "ps2_runtime_asset_resolver.h"
#include <algorithm>
#include <cstdint>
#include <span>
#include <unordered_map>
#include <vector>

namespace ps2::xbox360::resources {
enum class AssetServiceStatus : std::uint8_t { Ok, UnknownAsset, LoadFailed, InvalidHandle, UnsupportedRead };
struct AssetHandle { std::uint32_t value=0; [[nodiscard]] explicit operator bool()const noexcept{return value!=0;} };
struct AssetServiceResult { AssetServiceStatus status=AssetServiceStatus::UnknownAsset; AssetHandle handle{}; iop::AfsResourceId id=iop::AfsResourceId::Invalid; iop::AfsResourceType type=iop::AfsResourceType::Unknown; };
class AssetService {
 struct Entry { iop::AfsResourceId id=iop::AfsResourceId::Invalid; iop::AfsResourceType type=iop::AfsResourceType::Unknown; std::uint32_t refs=0; ResourceLoadingRuntime::Blob transient; const ResourceLoadingRuntime::Blob* cached=nullptr; };
public:
 explicit AssetService(RuntimeAssetResolver&resolver):resolver_(resolver){}
 [[nodiscard]] AssetServiceResult acquire(iop::AfsResourceId id){
  auto it=byId_.find(static_cast<std::uint32_t>(id));
  if(it!=byId_.end()){auto& e=entries_.at(it->second);++e.refs;return {AssetServiceStatus::Ok,{it->second},e.id,e.type};}
  ResourceLoadingRuntime::Blob tmp;auto loaded=resolver_.load(id,tmp);
  if(loaded.status!=AssetResolveStatus::Resolved)return {loaded.status==AssetResolveStatus::UnknownId?AssetServiceStatus::UnknownAsset:AssetServiceStatus::LoadFailed,{},id};
  const auto h=nextHandle();Entry e;e.id=id;e.type=loaded.type;e.refs=1;e.transient=std::move(tmp);e.cached=loaded.cached;
  entries_.emplace(h,std::move(e));byId_[static_cast<std::uint32_t>(id)]=h;return {AssetServiceStatus::Ok,{h},id,loaded.type};
 }
 [[nodiscard]] std::int32_t read(AssetHandle h,std::uint64_t offset,std::span<std::uint8_t>out){
  auto it=entries_.find(h.value);if(it==entries_.end())return -1;const auto&e=it->second;
  if(e.cached){if(offset>=e.cached->size()||out.empty())return 0;auto n=std::min(out.size(),e.cached->size()-static_cast<std::size_t>(offset));std::copy_n(e.cached->begin()+static_cast<std::ptrdiff_t>(offset),n,out.begin());return static_cast<std::int32_t>(n);}
  if(!e.transient.empty()){if(offset>=e.transient.size()||out.empty())return 0;auto n=std::min(out.size(),e.transient.size()-static_cast<std::size_t>(offset));std::copy_n(e.transient.begin()+static_cast<std::ptrdiff_t>(offset),n,out.begin());return static_cast<std::int32_t>(n);}
  return resolver_.read(e.id,offset,out);
 }
 [[nodiscard]] bool release(AssetHandle h){auto it=entries_.find(h.value);if(it==entries_.end())return false;if(--it->second.refs)return true;byId_.erase(static_cast<std::uint32_t>(it->second.id));entries_.erase(it);return true;}
 [[nodiscard]] std::uint32_t refCount(AssetHandle h)const{auto it=entries_.find(h.value);return it==entries_.end()?0:it->second.refs;}
 [[nodiscard]] std::size_t activeHandles()const noexcept{return entries_.size();}
 void clear(){entries_.clear();byId_.clear();next_=1;}
private:
 [[nodiscard]] std::uint32_t nextHandle(){while(next_==0||entries_.contains(next_))++next_;return next_++;}
 RuntimeAssetResolver&resolver_;std::uint32_t next_=1;std::unordered_map<std::uint32_t,Entry>entries_;std::unordered_map<std::uint32_t,std::uint32_t>byId_;
};
} // namespace ps2::xbox360::resources
