#include "ps2_resource_afs_catalog_manager.h"
#include <array>
#include <cassert>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <vector>
static void put32(std::vector<std::uint8_t>&b,std::size_t at,std::uint32_t v){for(int i=0;i<4;++i)b[at+i]=static_cast<std::uint8_t>(v>>(8*i));}
static void makeAfs(const std::filesystem::path&p,std::uint32_t count,const std::vector<std::uint8_t>&data){
 std::vector<std::uint8_t>b(8+count*8+data.size());b[0]='A';b[1]='F';b[2]='S';put32(b,4,count);
 for(std::uint32_t i=0;i<count;++i){put32(b,8+i*8,8+count*8);put32(b,12+i*8,static_cast<std::uint32_t>(data.size()));}
 std::copy(data.begin(),data.end(),b.begin()+static_cast<std::ptrdiff_t>(8+count*8));
 std::ofstream f(p,std::ios::binary);f.write(reinterpret_cast<const char*>(b.data()),static_cast<std::streamsize>(b.size()));
}
int main(){
 namespace fs=std::filesystem;using namespace ps2::xbox360::iop;using namespace ps2::xbox360::resources;
 const auto root=fs::temp_directory_path()/"mksm_catalog_resource_manager";fs::remove_all(root);fs::create_directories(root);
 makeAfs(root/"GAMEDATA.WAG",1,{1,2,3,4,5});makeAfs(root/"GAMEDATA.WAH",1,{6,7,8});makeAfs(root/"GAMEDATA.WAI",1,{9,10,11,12});makeAfs(root/"GAMEDATA.WAJ",1,{13,14,15,16,17,18});
 constexpr std::array records{
  AfsResourceRecord{static_cast<AfsResourceId>(101),AfsArchiveId::WAG,0,AfsResourceType::Texture,5,AfsLoadStrategy::Cache},
  AfsResourceRecord{static_cast<AfsResourceId>(102),AfsArchiveId::WAH,0,AfsResourceType::Model,3,AfsLoadStrategy::OnDemand},
  AfsResourceRecord{static_cast<AfsResourceId>(103),AfsArchiveId::WAI,0,AfsResourceType::Audio,4,AfsLoadStrategy::Stream},
  AfsResourceRecord{static_cast<AfsResourceId>(104),AfsArchiveId::WAJ,0,AfsResourceType::Animation,6,AfsLoadStrategy::SeekAware}};
 AfsResourceCatalog catalog(records);assert(catalog.valid());
 AfsStreamIo wag(root),wah(root),wai(root),waj(root);ResourceLoadingRuntime runtime(16);
 AfsCatalogResourceManager manager(catalog,wag,wah,wai,waj,runtime);
 auto cache=manager.loadCached(static_cast<AfsResourceId>(101));assert(cache&&cache->size()==5&&(*cache)[0]==1);
 assert(manager.loadCached(static_cast<AfsResourceId>(102))==nullptr);
 ResourceLoadingRuntime::Blob transient;assert(manager.loadOnDemand(static_cast<AfsResourceId>(102),transient)&&transient.size()==3&&transient[0]==6);
 std::array<std::uint8_t,3> audio{};assert(manager.read(static_cast<AfsResourceId>(103),1,audio)==3);
 assert((audio==std::array<std::uint8_t,3>{10,11,12}));
 std::array<std::uint8_t,2> anim{};assert(manager.read(static_cast<AfsResourceId>(104),4,anim)==2);
 assert((anim==std::array<std::uint8_t,2>{17,18}));
 assert(manager.resolve(static_cast<AfsResourceId>(104))!=nullptr);
 assert(manager.findByEntry(AfsArchiveId::WAI,0)->id==static_cast<AfsResourceId>(103);
 fs::remove_all(root);
}
