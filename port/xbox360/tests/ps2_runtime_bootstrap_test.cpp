#include "ps2_runtime_bootstrap.h"

#include <array>
#include <cassert>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <vector>

int main()
{
    namespace fs = std::filesystem;
    using namespace ps2::xbox360;
    using iop::IomanCommand;

    const fs::path root = fs::temp_directory_path() / "mksm_runtime_bootstrap_test";
    fs::remove_all(root);
    fs::create_directories(root);
    {
        std::ofstream file(root / "boot.bin", std::ios::binary);
        constexpr std::array<std::uint8_t, 5> payload{0x4D, 0x4B, 0x53, 0x4D, 0x21};
        file.write(reinterpret_cast<const char *>(payload.data()), payload.size());
    }

    PS2RuntimeBootstrap bootstrap(root);
    constexpr std::uint32_t kOpen = 0x51u;
    constexpr std::uint32_t kRead = 0x52u;
    constexpr std::uint32_t kSeek = 0x53u;
    constexpr std::uint32_t kClose = 0x54u;
    assert(bootstrap.bindIomanSyscall(kOpen, IomanCommand::Open));
    assert(bootstrap.bindIomanSyscall(kRead, IomanCommand::Read));
    assert(bootstrap.bindIomanSyscall(kSeek, IomanCommand::Lseek));
    assert(bootstrap.bindIomanSyscall(kClose, IomanCommand::Close));
    assert(!bootstrap.bindIomanSyscall(kOpen, IomanCommand::Read));

    std::vector<std::uint8_t> rdram(rdram_address::kRdramSize);
    constexpr std::uint32_t pathAddress = 0x20000020u;
    constexpr std::uint32_t bufferAddress = 0x20000100u;
    constexpr char path[] = "boot.bin";
    for (std::size_t i = 0; i < sizeof(path); ++i)
        rdram[0x20u + i] = static_cast<std::uint8_t>(path[i]);

    const auto fd = bootstrap.dispatchEeIomanSyscall(kOpen, pathAddress, 1u, 0u, rdram);
    assert(fd >= iop::RetailFileIo::FirstDescriptor);
    assert(bootstrap.dispatchEeIomanSyscall(kRead, static_cast<std::uint32_t>(fd),
                                            bufferAddress, 5u, rdram) == 5);
    constexpr std::array<std::uint8_t, 5> expected{0x4D, 0x4B, 0x53, 0x4D, 0x21};
    for (std::size_t i = 0; i < expected.size(); ++i)
        assert(rdram[0x100u + i] == expected[i]);
    assert(bootstrap.dispatchEeIomanSyscall(kSeek, static_cast<std::uint32_t>(fd),
                                            0u, 0u, rdram) == 0);
    assert(bootstrap.dispatchEeIomanSyscall(kClose, static_cast<std::uint32_t>(fd),
                                            0u, 0u, rdram) == 0);
    assert(bootstrap.ioman().openFileCount() == 0u);
    assert(bootstrap.trace().size() == 4u);
    assert(bootstrap.dispatchEeIomanSyscall(0x99u, 0u, 0u, 0u, rdram) == -ENOSYS);

    bootstrap.reset();
    fs::remove_all(root);
    return 0;
}
