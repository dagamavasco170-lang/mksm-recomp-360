#include "ps2_resource_afs_bridge.h"
#include <array>
#include <cassert>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <vector>
static void put32(std::vector<std::uint8_t>&b,std::size_t at,std::uint32_t v){b[at]=static_cast<std::uint8_t>(v);b[at+1]=static_cast<std::uint8_t>(v>>8);b[at+2]=static_cast<std::uint8_t>(v>>16);b[at+3]=static_cast<std::uint8_t>(v>>24);}
int main(){
 namespace fs=std::filesystem;using namespace ps2::xbox360::resources;
 const auto root=fs::temp_directory_path()/"mksm_resource_afs_bridge_test";fs::remove_all(root);fs::create_directories(root);
 std::vector<std::uint8_t> afs(40,0);afs[0]='A';afs[1]='F';afs[2]='S';put32(afs,4,2);
 put32(afs,8,24);put32(afs,12,4);put32(afs,16,28);put32(afs,20,12);
 for(std::size_t i=0;i<16;++i)afs[24+i]=static_cast<std::uint8_t>(0xA0+i);
 {std::ofstream f(root/"GAMEDATA.WAG",std::ios::binary);f.write(reinterpret_cast<const char*>(afs.data()),static_cast<std::streamsize>(afs.size()));}
 ps2::xbox360::iop::AfsStreamIo afsIo(root);AfsResourceBridge bridge(afsIo,"GAMEDATA.WAG");
 ResourceLoadingRuntime runtime(16);const auto*cached=runtime.loadCached(7,bridge.loader(1));
 assert(cached&&cached->size()==12&&(*cached)[0]==0xA4&&(*cached)[11]==0xAF);
 assert(runtime.cacheUsed()==12);
 const auto*hit=runtime.loadCached(7,bridge.loader(1));assert(hit==cached&&runtime.stats().cacheHits==1);
 std::array<std::uint8_t,4> block{};assert(runtime.stream(8,3,block,bridge.streamReader(1))==4);
 assert((block==std::array<std::uint8_t,4>{0xA7,0xA8,0xA9,0xAA}));
 ResourceLoadingRuntime::Blob transient;assert(runtime.loadOnDemand(9,bridge.loader(0),transient));
 assert(transient.size()==4&&transient[0]==0xA0&&runtime.cacheUsed()==12);
 runtime.clear();assert(runtime.cacheUsed()==0);fs::remove_all(root);
}
