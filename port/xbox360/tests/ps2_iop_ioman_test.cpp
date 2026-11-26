#include "ps2_iop_ioman.h"

#include <array>
#include <cassert>
#include <cerrno>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <span>
#include <vector>

int main()
{
    namespace fs = std::filesystem;
    using ps2::xbox360::iop::IomanCommand;
    using ps2::xbox360::iop::IomanDispatcher;

    const fs::path root = fs::temp_directory_path() / "mksm_ioman_dispatch_test";
    fs::remove_all(root);
    fs::create_directories(root / "IOP");
    {
        std::ofstream file(root / "IOP" / "MODULE.IRX", std::ios::binary);
        constexpr std::array<std::uint8_t, 6> data{1, 2, 3, 4, 5, 6};
        file.write(reinterpret_cast<const char *>(data.data()),
                   static_cast<std::streamsize>(data.size()));
    }

    IomanDispatcher ioman(root);
    std::array<std::uint8_t, 128> ram{};
    constexpr char path[] = "cdrom0:\\\\iop\\\\module.irx;1";
    for (std::size_t i = 0; i < sizeof(path); ++i)
        ram[8u + i] = static_cast<std::uint8_t>(path[i]);

    const auto fd = ioman.dispatch(IomanCommand::Open, 8u, 1u, 0u, ram);
    assert(fd == 3);
    assert(ioman.openFileCount() == 1u);

    assert(ioman.dispatch(IomanCommand::Read, static_cast<std::uint32_t>(fd),
                          64u, 4u, ram) == 4);
    assert((std::array<std::uint8_t, 4>{ram[64], ram[65], ram[66], ram[67]} ==
            std::array<std::uint8_t, 4>{1, 2, 3, 4}));

    assert(ioman.dispatch(IomanCommand::Lseek, static_cast<std::uint32_t>(fd),
                          static_cast<std::uint32_t>(-2), 1u, ram) == 2);
    assert(ioman.dispatch(IomanCommand::Read, static_cast<std::uint32_t>(fd),
                          80u, 8u, ram) == 4);
    assert((std::array<std::uint8_t, 4>{ram[80], ram[81], ram[82], ram[83]} ==
            std::array<std::uint8_t, 4>{3, 4, 5, 6}));

    // Zero-length reads are successful no-ops, including at EOF.
    assert(ioman.dispatch(IomanCommand::Read, static_cast<std::uint32_t>(fd),
                          127u, 0u, ram) == 0);
    assert(ioman.dispatch(IomanCommand::Lseek, static_cast<std::uint32_t>(fd),
                          static_cast<std::uint32_t>(-2), 2u, ram) == 4);
    assert(ioman.dispatch(IomanCommand::Read, static_cast<std::uint32_t>(fd),
                          96u, 2u, ram) == 2);
    assert((std::array<std::uint8_t, 2>{ram[96], ram[97]} ==
            std::array<std::uint8_t, 2>{5, 6}));
    assert(ioman.dispatch(IomanCommand::Lseek, static_cast<std::uint32_t>(fd),
                          6u, 0u, ram) == 6);
    assert(ioman.dispatch(IomanCommand::Read, static_cast<std::uint32_t>(fd),
                          96u, 2u, ram) == 0); // EOF
    assert(ioman.dispatch(IomanCommand::Lseek, static_cast<std::uint32_t>(fd),
                          static_cast<std::uint32_t>(-1), 0u, ram) == -EINVAL);
    assert(ioman.dispatch(IomanCommand::Lseek, static_cast<std::uint32_t>(fd),
                          0u, 3u, ram) == -EINVAL);
    assert(ioman.dispatch(IomanCommand::Read, 999u, 96u, 1u, ram) == -EBADF);

    assert(ioman.dispatch(IomanCommand::Read, static_cast<std::uint32_t>(fd),
                          127u, 2u, ram) == -EFAULT);
    assert(ioman.dispatch(IomanCommand::Open, 127u, 0u, 0u, ram) == -EFAULT);
    assert(ioman.dispatch(IomanCommand::Open, 8u, 0u, 0u, ram) == -EACCES);
    assert(ioman.dispatch(static_cast<IomanCommand>(99), 0u, 0u, 0u, ram) == -ENOSYS);
    assert(ioman.dispatch(IomanCommand::Close, static_cast<std::uint32_t>(fd),
                          0u, 0u, ram) == 0);
    assert(ioman.openFileCount() == 0u);
    assert(ioman.dispatch(IomanCommand::Close, static_cast<std::uint32_t>(fd),
                          0u, 0u, ram) == -EBADF);

    // Route an AFS archive-entry path through the dispatcher.
    {
        std::vector<std::uint8_t> archive(20, 0);
        archive[0] = 'A'; archive[1] = 'F'; archive[2] = 'S'; archive[3] = 0;
        archive[4] = 1; // one entry
        archive[8] = 16; // payload offset
        archive[12] = 4; // payload size
        archive[16] = 0xCA; archive[17] = 0xFE;
        archive[18] = 0xBA; archive[19] = 0xBE;
        std::ofstream afs(root / "GAMEDATA.WAG", std::ios::binary);
        afs.write(reinterpret_cast<const char *>(archive.data()),
                  static_cast<std::streamsize>(archive.size()));
    }
    constexpr char afsPath[] = "afs0:/GAMEDATA.WAG/0";
    for (std::size_t i = 0; i < sizeof(afsPath); ++i)
        ram[8u + i] = static_cast<std::uint8_t>(afsPath[i]);
    const auto afsFd = ioman.dispatch(IomanCommand::Open, 8u, 1u, 0u, ram);
    assert(afsFd >= ps2::xbox360::iop::AfsStreamIo::FirstDescriptor);
    assert(ioman.dispatch(IomanCommand::Read, static_cast<std::uint32_t>(afsFd),
                         64u, 4u, ram) == 4);
    assert((std::array<std::uint8_t, 4>{ram[64], ram[65], ram[66], ram[67]} ==
            std::array<std::uint8_t, 4>{0xCA, 0xFE, 0xBA, 0xBE}));
    assert(ioman.dispatch(IomanCommand::Close, static_cast<std::uint32_t>(afsFd),
                         0u, 0u, ram) == 0);

    // Resolve a logical resource ID through the catalog into the same streaming AFS IOMAN.
    constexpr std::array catalogRecords{
        ps2::xbox360::iop::AfsResourceRecord{
            static_cast<ps2::xbox360::iop::AfsResourceId>(1),
            ps2::xbox360::iop::AfsArchiveId::WAG, 0,
            ps2::xbox360::iop::AfsResourceType::Other, 4}
    };
    constexpr ps2::xbox360::iop::AfsResourceCatalog catalog{catalogRecords};
    const auto catalogFd = ioman.openAfsResource(
        catalog, static_cast<ps2::xbox360::iop::AfsResourceId>(1));
    assert(catalogFd >= ps2::xbox360::iop::AfsStreamIo::FirstDescriptor);
    assert(ioman.dispatch(IomanCommand::Read, static_cast<std::uint32_t>(catalogFd),
                         64u, 4u, ram) == 4);
    assert((std::array<std::uint8_t, 4>{ram[64], ram[65], ram[66], ram[67]} ==
            std::array<std::uint8_t, 4>{0xCA, 0xFE, 0xBA, 0xBE}));
    assert(ioman.dispatch(IomanCommand::Close, static_cast<std::uint32_t>(catalogFd),
                         0u, 0u, ram) == 0);
    assert(ioman.openAfsResource(catalog, static_cast<ps2::xbox360::iop::AfsResourceId>(99)) == -ENOENT);

    // Restore a normal disc path before exhausting the retail descriptor table.
    for (std::size_t i = 0; i < sizeof(path); ++i)
        ram[8u + i] = static_cast<std::uint8_t>(path[i]);

    // Exhaust the descriptor table and verify deterministic EMFILE behavior.
    std::array<std::int32_t, ps2::xbox360::iop::RetailFileIo::MaxOpenFiles> descriptors{};
    for (auto &descriptor : descriptors)
    {
        descriptor = ioman.dispatch(IomanCommand::Open, 8u, 1u, 0u, ram);
        assert(descriptor >= ps2::xbox360::iop::RetailFileIo::FirstDescriptor);
    }
    assert(ioman.openFileCount() == ps2::xbox360::iop::RetailFileIo::MaxOpenFiles);
    assert(ioman.dispatch(IomanCommand::Open, 8u, 1u, 0u, ram) == -EMFILE);
    for (const auto descriptor : descriptors)
        assert(ioman.dispatch(IomanCommand::Close, static_cast<std::uint32_t>(descriptor),
                             0u, 0u, ram) == 0);
    assert(ioman.openFileCount() == 0u);

    fs::remove_all(root);
    return 0;
}
