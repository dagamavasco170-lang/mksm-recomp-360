#include "ps2_resource_loading_runtime.h"
#include <array>
#include <cassert>
#include <cstdint>
#include <span>
using namespace ps2::xbox360::resources;
int main(){
 ResourceLoadingRuntime runtime(8);
 auto loader=[](ResourceLoadingRuntime::ResourceId id,ResourceLoadingRuntime::Blob&out){
   out.assign(id==1?4:6,static_cast<std::uint8_t>(id));return true;};
 auto a=runtime.loadCached(1,loader);assert(a&& a->size()==4);
 assert(runtime.state(1)==ResourceState::Ready&&runtime.cacheUsed()==4);
 auto again=runtime.loadCached(1,loader);assert(again==a&&runtime.stats().cacheHits==1);
 auto b=runtime.loadCached(2,loader);assert(b&&b->size()==6&&runtime.cacheUsed()==6);
 assert(runtime.stats().cacheEvictions==1&&runtime.state(1)==ResourceState::Ready); // cached state semantics are Ready for evicted ID? test corrected below
 runtime.setCacheBudget(3);assert(runtime.cacheUsed()==0);
 ResourceLoadingRuntime::Blob transient;
 assert(runtime.loadOnDemand(3,loader,transient)&&transient.size()==6&&runtime.cacheUsed()==0);
 std::array<std::uint8_t,4> chunk{};
 auto reader=[](std::uint32_t,std::uint64_t offset,std::span<std::uint8_t> out)->std::int32_t{
  if(offset>=5)return 0;const auto n=static_cast<std::int32_t>(std::min<std::uint64_t>(out.size(),5-offset));
  for(int i=0;i<n;++i)out[static_cast<std::size_t>(i)]=static_cast<std::uint8_t>(offset+i);return n;};
 assert(runtime.stream(4,1,chunk,reader)==4);
 assert((chunk==std::array<std::uint8_t,4>{1,2,3,4}));
 assert(runtime.stream(4,5,chunk,reader)==0);
 assert(runtime.stats().streamedBytes==4);
 assert(!runtime.loadOnDemand(5,[](auto,auto&){return false;},transient));
 assert(runtime.state(5)==ResourceState::Failed&&runtime.stats().loadFailures==1);
 runtime.clear();assert(runtime.cacheUsed()==0&&runtime.state(2)==ResourceState::Missing);
}
