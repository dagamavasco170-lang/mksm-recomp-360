#include "ps2_runtime_bootstrap.h"

#include <array>
#include <cassert>
#include <cstdint>
#include <filesystem>
#include <fstream>

namespace
{
bool g_entrySawExpectedRuntime = false;

void generatedFixture(std::uint8_t *rdram, R5900Context *context, PS2Runtime *runtime)
{
    g_entrySawExpectedRuntime = rdram != nullptr && context != nullptr && runtime != nullptr;
    rdram[0x180u] = 0xA5u;
    context->pc = 0x00102030u;
    context->r[2].low64 = 0x1234u;
}
}

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

    auto ram = bootstrap.eeRdram();
    constexpr std::uint32_t pathAddress = 0x20000020u;
    constexpr std::uint32_t bufferAddress = 0x20000100u;
    constexpr char path[] = "boot.bin";
    for (std::size_t i = 0; i < sizeof(path); ++i)
        ram[0x20u + i] = static_cast<std::uint8_t>(path[i]);

    const auto fd = bootstrap.dispatchEeIomanSyscall(kOpen, pathAddress, 1u, 0u);
    assert(fd >= iop::RetailFileIo::FirstDescriptor);
    assert(bootstrap.dispatchEeIomanSyscall(kRead, static_cast<std::uint32_t>(fd),
                                            bufferAddress, 5u) == 5);
    constexpr std::array<std::uint8_t, 5> expected{0x4D, 0x4B, 0x53, 0x4D, 0x21};
    for (std::size_t i = 0; i < expected.size(); ++i)
        assert(ram[0x100u + i] == expected[i]);
    assert(bootstrap.dispatchEeIomanSyscall(kSeek, static_cast<std::uint32_t>(fd),
                                            0u, 0u) == 0);
    assert(bootstrap.dispatchEeIomanSyscall(kClose, static_cast<std::uint32_t>(fd),
                                            0u, 0u) == 0);
    assert(bootstrap.ioman().openFileCount() == 0u);
    assert(bootstrap.trace().size() == 4u);

    // Context register convention: a0=path, a1=flags, a2=unused, v0=result.
    for (std::size_t i = 0; i < sizeof(path); ++i)
        ram[0x20u + i] = static_cast<std::uint8_t>(path[i]);
    bootstrap.context().r[4].low64 = pathAddress;
    bootstrap.context().r[5].low64 = 1u;
    bootstrap.context().r[6].low64 = 0u;
    const auto contextFd = bootstrap.dispatchIomanFromContext(kOpen);
    assert(contextFd >= iop::RetailFileIo::FirstDescriptor);
    assert(static_cast<std::int32_t>(bootstrap.context().r[2].low64) == contextFd);
    assert(bootstrap.dispatchEeIomanSyscall(kClose, static_cast<std::uint32_t>(contextFd),
                                            0u, 0u) == 0);

    assert(bootstrap.invokeGeneratedEntry(nullptr) == GeneratedEntryStatus::NullEntry);
    assert(bootstrap.invokeGeneratedEntry(&generatedFixture) == GeneratedEntryStatus::Invoked);
    assert(g_entrySawExpectedRuntime);
    assert(bootstrap.eeRdram()[0x180u] == 0xA5u);
    assert(bootstrap.context().pc == 0x00102030u);
    assert(bootstrap.context().r[2].low64 == 0x1234u);

    assert(bootstrap.dispatchEeIomanSyscall(0x99u, 0u, 0u, 0u) == -ENOSYS);
    bootstrap.reset();
    assert(bootstrap.context().pc == 0u);
    assert(bootstrap.eeRdram()[0x180u] == 0u);
    fs::remove_all(root);
    return 0;
}
