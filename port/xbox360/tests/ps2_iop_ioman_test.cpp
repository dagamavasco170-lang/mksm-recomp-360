#include "ps2_iop_ioman.h"

#include <array>
#include <cassert>
#include <cerrno>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <span>

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
    constexpr char path[] = "IOP/MODULE.IRX";
    for (std::size_t i = 0; i < sizeof(path); ++i)
        ram[8u + i] = static_cast<std::uint8_t>(path[i]);

    const auto fd = ioman.dispatch(IomanCommand::Open, 8u, 0u, 0u, ram);
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

    assert(ioman.dispatch(IomanCommand::Read, static_cast<std::uint32_t>(fd),
                          127u, 2u, ram) == -EFAULT);
    assert(ioman.dispatch(IomanCommand::Open, 127u, 0u, 0u, ram) == -EFAULT);
    assert(ioman.dispatch(static_cast<IomanCommand>(99), 0u, 0u, 0u, ram) == -ENOSYS);
    assert(ioman.dispatch(IomanCommand::Close, static_cast<std::uint32_t>(fd),
                          0u, 0u, ram) == 0);
    assert(ioman.openFileCount() == 0u);
    assert(ioman.dispatch(IomanCommand::Close, static_cast<std::uint32_t>(fd),
                          0u, 0u, ram) == -EBADF);

    fs::remove_all(root);
    return 0;
}
