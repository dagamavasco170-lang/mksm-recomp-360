#include "ps2_iop_ioman.h"

#include <array>
#include <cassert>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <vector>

static void put32(std::vector<std::uint8_t>& b, std::size_t at, std::uint32_t v)
{
    for (int i = 0; i < 4; ++i)
        b[at + static_cast<std::size_t>(i)] = static_cast<std::uint8_t>(v >> (8 * i));
}

static void makeAfs(const std::filesystem::path& p)
{
    std::vector<std::uint8_t> b(32, 0);
    b[0] = 'A'; b[1] = 'F'; b[2] = 'S';
    put32(b, 4, 2);       // two entries
    put32(b, 8, 24); put32(b, 12, 4);
    put32(b, 16, 28); put32(b, 20, 4);
    b[24] = 0x10; b[25] = 0x20; b[26] = 0x30; b[27] = 0x40;
    b[28] = 0xAA; b[29] = 0xBB; b[30] = 0xCC; b[31] = 0xDD;
    std::ofstream f(p, std::ios::binary);
    f.write(reinterpret_cast<const char*>(b.data()), static_cast<std::streamsize>(b.size()));
}

int main()
{
    namespace fs = std::filesystem;
    using namespace ps2::xbox360::iop;
    using namespace ps2::xbox360::resources;
    const fs::path root = fs::temp_directory_path() / "mksm_ioman_asset_bridge_test";
    fs::remove_all(root);
    fs::create_directories(root);
    makeAfs(root / "GAMEDATA.WAG");

    constexpr std::array records{
        AfsResourceRecord{static_cast<AfsResourceId>(1), AfsArchiveId::WAG, 0,
                         AfsResourceType::Texture, 4, AfsLoadStrategy::Cache}
    };
    AfsResourceCatalog catalog(records);
    AfsStreamIo wag(root), wah(root), wai(root), waj(root);
    ResourceLoadingRuntime runtime(4);
    AfsCatalogResourceManager manager(catalog, wag, wah, wai, waj, runtime);
    RuntimeAssetResolver resolver(manager);
    AssetService service(resolver);
    IomanDispatcher ioman(root);
    ioman.setAssetService(&service);

    std::array<std::uint8_t, 128> ram{};
    constexpr char catalogPath[] = "afs0:/GAMEDATA.WAG/0";
    for (std::size_t i = 0; i < sizeof(catalogPath); ++i)
        ram[8 + i] = static_cast<std::uint8_t>(catalogPath[i]);
    const auto fd = ioman.dispatch(IomanCommand::Open, 8, RetailFileIo::OpenReadOnly, 0, ram);
    assert(fd == IomanDispatcher::AssetServiceFirstDescriptor);
    assert(ioman.dispatch(IomanCommand::Read, static_cast<std::uint32_t>(fd), 64, 4, ram) == 4);
    assert((std::array<std::uint8_t, 4>{ram[64], ram[65], ram[66], ram[67]} ==
            std::array<std::uint8_t, 4>{0x10, 0x20, 0x30, 0x40}));
    assert(ioman.dispatch(IomanCommand::Lseek, static_cast<std::uint32_t>(fd),
                         static_cast<std::uint32_t>(-2), 2, ram) == 2);
    assert(ioman.dispatch(IomanCommand::Read, static_cast<std::uint32_t>(fd), 80, 2, ram) == 2);
    assert((std::array<std::uint8_t, 2>{ram[80], ram[81]} == std::array<std::uint8_t, 2>{0x30, 0x40}));
    assert(ioman.dispatch(IomanCommand::Close, static_cast<std::uint32_t>(fd), 0, 0, ram) == 0);
    assert(service.activeHandles() == 0);

    // Uncataloged entries stay on the original direct-stream path.
    constexpr char fallbackPath[] = "afs0:/GAMEDATA.WAG/1";
    for (std::size_t i = 0; i < sizeof(fallbackPath); ++i)
        ram[8 + i] = static_cast<std::uint8_t>(fallbackPath[i]);
    const auto fallbackFd = ioman.dispatch(IomanCommand::Open, 8, RetailFileIo::OpenReadOnly, 0, ram);
    assert(fallbackFd >= AfsStreamIo::FirstDescriptor && fallbackFd < IomanDispatcher::AssetServiceFirstDescriptor);
    assert(ioman.dispatch(IomanCommand::Read, static_cast<std::uint32_t>(fallbackFd), 64, 4, ram) == 4);
    assert((std::array<std::uint8_t, 4>{ram[64], ram[65], ram[66], ram[67]} ==
            std::array<std::uint8_t, 4>{0xAA, 0xBB, 0xCC, 0xDD}));
    assert(ioman.dispatch(IomanCommand::Close, static_cast<std::uint32_t>(fallbackFd), 0, 0, ram) == 0);

    fs::remove_all(root);
    return 0;
}
