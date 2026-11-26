#pragma once
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <list>
#include <span>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace ps2::xbox360::resources
{
// Runtime primitives only: policy inputs are metadata and never trusted to identify assets.
enum class LoadStrategy : std::uint8_t { Cache, Stream, SeekAware, OnDemand };
enum class ResourceState : std::uint8_t { Missing, Loading, Ready, Evicted, Failed };
struct ResourceRuntimeStats {
    std::size_t cacheBudgetBytes=0, cacheUsedBytes=0, cacheHits=0, cacheMisses=0,
                cacheEvictions=0, loadFailures=0, streamedBytes=0;
};
class ResourceLoadingRuntime {
public:
    using ResourceId=std::uint32_t;
    using Blob=std::vector<std::uint8_t>;
    using Loader=std::function<bool(ResourceId,Blob&)>;
    using StreamReader=std::function<std::int32_t(ResourceId,std::uint64_t,std::span<std::uint8_t>)>;
    explicit ResourceLoadingRuntime(std::size_t budgetBytes):budget_(budgetBytes){}
    void setCacheBudget(std::size_t bytes){budget_=bytes;evictToBudget();}
    [[nodiscard]] std::size_t cacheBudget()const noexcept{return budget_;}
    [[nodiscard]] std::size_t cacheUsed()const noexcept{return used_;}
    [[nodiscard]] ResourceRuntimeStats stats()const noexcept{return stats_;}
    [[nodiscard]] ResourceState state(ResourceId id)const noexcept {
        if(cache_.contains(id))return ResourceState::Ready;
        const auto it=states_.find(id);return it==states_.end()?ResourceState::Missing:it->second;
    }
    // Cache only admits complete blobs that fit; loader failures never create entries.
    [[nodiscard]] const Blob* loadCached(ResourceId id,const Loader& loader){
        auto found=cache_.find(id);
        if(found!=cache_.end()){touch(found);++stats_.cacheHits;return &found->second.data;}
        ++stats_.cacheMisses;states_[id]=ResourceState::Loading;
        Blob data;
        if(!loader||!loader(id,data)){states_[id]=ResourceState::Failed;++stats_.loadFailures;return nullptr;}
        if(data.size()>budget_){states_[id]=ResourceState::Failed;return nullptr;}
        evictFor(data.size());
        lru_.push_front(id);CacheEntry entry{std::move(data),lru_.begin()};
        used_+=entry.data.size();auto [it,inserted]=cache_.emplace(id,std::move(entry));
        if(!inserted){lru_.pop_front();states_[id]=ResourceState::Ready;return &it->second.data;}
        states_[id]=ResourceState::Ready;return &it->second.data;
    }
    // ON_DEMAND: load transiently; caller owns the returned bytes.
    [[nodiscard]] bool loadOnDemand(ResourceId id,const Loader&loader,Blob&out){
        states_[id]=ResourceState::Loading;out.clear();
        if(!loader||!loader(id,out)){out.clear();states_[id]=ResourceState::Failed;++stats_.loadFailures;return false;}
        states_[id]=ResourceState::Ready;return true;
    }
    // STREAM: requests bounded chunks. A short positive read is valid; zero is EOF.
    [[nodiscard]] std::int32_t stream(ResourceId id,std::uint64_t offset,std::span<std::uint8_t> out,const StreamReader&reader){
        if(!reader){states_[id]=ResourceState::Failed;++stats_.loadFailures;return -1;}
        states_[id]=ResourceState::Loading;auto n=reader(id,offset,out);
        if(n<0){states_[id]=ResourceState::Failed;++stats_.loadFailures;return n;}
        if(static_cast<std::size_t>(n)>out.size()){states_[id]=ResourceState::Failed;++stats_.loadFailures;return -1;}
        stats_.streamedBytes+=static_cast<std::size_t>(n);states_[id]=ResourceState::Ready;return n;
    }
    void erase(ResourceId id){auto it=cache_.find(id);if(it!=cache_.end())eraseEntry(it);states_.erase(id);}
    void clear(){cache_.clear();lru_.clear();states_.clear();used_=0;}
private:
    struct CacheEntry{Blob data;std::list<ResourceId>::iterator lruIt;};
    using MapIt=std::unordered_map<ResourceId,CacheEntry>::iterator;
    void touch(MapIt it){lru_.splice(lru_.begin(),lru_,it->second.lruIt);it->second.lruIt=lru_.begin();}
    void eraseEntry(MapIt it){const auto id=it->first;used_-=it->second.data.size();lru_.erase(it->second.lruIt);cache_.erase(it);states_[id]=ResourceState::Evicted;++stats_.cacheEvictions;}
    void evictFor(std::size_t needed){while(used_+needed>budget_&&!lru_.empty()){auto it=cache_.find(lru_.back());if(it!=cache_.end())eraseEntry(it);else lru_.pop_back();}}
    void evictToBudget(){evictFor(0);}
    std::size_t budget_=0,used_=0;ResourceRuntimeStats stats_{};
    std::list<ResourceId> lru_;std::unordered_map<ResourceId,CacheEntry> cache_;
    std::unordered_map<ResourceId,ResourceState> states_;
};
} // namespace ps2::xbox360::resources
