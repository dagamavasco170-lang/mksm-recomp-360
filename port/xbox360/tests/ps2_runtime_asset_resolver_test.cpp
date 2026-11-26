#include "ps2_runtime_asset_resolver.h"
#include <array>
#include <cassert>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <vector>
static void put32(std::vector<std::uint8_t>&b,std::size_t at,std::uint32_t v){for(int i=0;i<4;++i)b[at+i]=static_cast<std::uint8_t>(v>>(8*i));}
static void makeAfs(const std::filesystem::path&p,const std::vector<std::uint8_t>&data){std::vector<std::uint8_t>b(16+data.size());b[0]='A';b[1]='F';b[2]='S';put32(b,4,1);put32(b,8,16);put32(b,12,static_cast<std::uint32_t>(data.size()));std::copy(data.begin(),data.end(),b.begin()+16);std::ofstream f(p,std::ios::binary);f.write(reinterpret_cast<const char*>(b.data()),b.size());}
int main(){namespace fs=std::filesystem;using namespace ps2::xbox360::iop;using namespace ps2::xbox360::resources;
 auto root=fs::temp_directory_path()/"mksm_asset_resolver";fs::remove_all(root);fs::create_directories(root);
 makeAfs(root/"GAMEDATA.WAG",{1,2,3,4});makeAfs(root/"GAMEDATA.WAH",{5,6,7});makeAfs(root/"GAMEDATA.WAI",{8,9});makeAfs(root/"GAMEDATA.WAJ",{10,11,12});
 constexpr std::array rec{AfsResourceRecord{static_cast<AfsResourceId>(1),AfsArchiveId::WAG,0,AfsResourceType::Texture,4,AfsLoadStrategy::Cache},AfsResourceRecord{static_cast<AfsResourceId>(2),AfsArchiveId::WAH,0,AfsResourceType::Model,3,AfsLoadStrategy::OnDemand},AfsResourceRecord{static_cast<AfsResourceId>(3),AfsArchiveId::WAI,0,AfsResourceType::Audio,2,AfsLoadStrategy::Stream},AfsResourceRecord{static_cast<AfsResourceId>(4),AfsArchiveId::WAJ,0,AfsResourceType::Animation,3,AfsLoadStrategy::SeekAware}};
 constexpr std::array deps{AssetDependency{static_cast<AfsResourceId>(1),static_cast<AfsResourceId>(2)},AssetDependency{static_cast<AfsResourceId>(2),static_cast<AfsResourceId>(3)}};
 AfsResourceCatalog catalog(rec);AfsStreamIo wag(root),wah(root),wai(root),waj(root);ResourceLoadingRuntime runtime(32);AfsCatalogResourceManager manager(catalog,wag,wah,wai,waj,runtime);RuntimeAssetResolver resolver(manager,deps);
 assert(resolver.resolve(static_cast<AfsResourceId>(1)).status==AssetResolveStatus::Resolved);
 auto byEntry=resolver.resolve(AfsArchiveId::WAI,0);assert(byEntry.id==static_cast<AfsResourceId>(3)&&byEntry.type==AfsResourceType::Audio);
 ResourceLoadingRuntime::Blob transient;auto loaded=resolver.load(static_cast<AfsResourceId>(1),transient);assert(loaded.cached&&loaded.cached->size()==4&&loaded.cached->at(0)==1);
 auto onDemand=resolver.load(static_cast<AfsResourceId>(2),transient);assert(onDemand.status==AssetResolveStatus::Resolved&&transient.size()==3&&transient[0]==5);
 std::array<std::uint8_t,2>stream{};assert(resolver.read(static_cast<AfsResourceId>(3),0,stream)==2&&stream[1]==9);
 assert(resolver.resolve(static_cast<AfsResourceId>(99)).status==AssetResolveStatus::UnknownId);
 assert(resolver.dependencyGraphValid());std::vector<AfsResourceId>closure;assert(resolver.dependencyClosure(static_cast<AfsResourceId>(1),closure)&&closure.size()==3);
 fs::remove_all(root);
}
