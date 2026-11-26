#include "ps2_generated_runtime_abi.h"

#include <array>
#include <cassert>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <vector>

int main()
{
    namespace fs = std::filesystem;
    using ps2::xbox360::iop::IomanCommand;
    const fs::path root = fs::temp_directory_path() / "mksm_ee_ioman_syscall_dispatch_test";
    fs::remove_all(root);
    fs::create_directories(root);
    {
        std::ofstream file(root / "syscall_map.bin", std::ios::binary);
        constexpr std::array<std::uint8_t, 4> data{0xFA, 0xCE, 0xB0, 0x0C};
        file.write(reinterpret_cast<const char*>(data.data()), data.size());
    }

    ps2::xbox360::iop::IomanDispatcher ioman(root);
    PS2Runtime runtime;
    runtime.attachIoman(&ioman);
    constexpr std::uint32_t testOpenId = 0x7001;
    constexpr std::uint32_t testReadId = 0x7002;
    constexpr std::uint32_t testSeekId = 0x7003;
    constexpr std::uint32_t testCloseId = 0x7004;
    assert(runtime.bindIomanSyscall(testOpenId, IomanCommand::Open));
    assert(runtime.bindIomanSyscall(testReadId, IomanCommand::Read));
    assert(runtime.bindIomanSyscall(testSeekId, IomanCommand::Lseek));
    assert(runtime.bindIomanSyscall(testCloseId, IomanCommand::Close));
    assert(!runtime.bindIomanSyscall(testOpenId, IomanCommand::Read)); // conflicting rebind

    std::vector<std::uint8_t> ram(ps2::xbox360::rdram_address::kRdramSize);
    constexpr std::uint32_t pathAddress = 0x3000;
    constexpr std::uint32_t readAddress = 0x4000;
    constexpr char pathText[] = "syscall_map.bin";
    for (std::size_t i = 0; i < sizeof(pathText); ++i)
        ram[pathAddress + i] = static_cast<std::uint8_t>(pathText[i]);

    assert(runtime.dispatchEeIomanSyscall(0x7FFF, 0, 0, 0, ram) == -ENOSYS);
    const auto fd = runtime.dispatchEeIomanSyscall(testOpenId, pathAddress, 1, 0, ram);
    assert(fd >= ps2::xbox360::iop::RetailFileIo::FirstDescriptor);
    assert(runtime.dispatchEeIomanSyscall(testReadId, static_cast<std::uint32_t>(fd),
                                         readAddress, 4, ram) == 4);
    assert((std::array<std::uint8_t, 4>{ram[readAddress], ram[readAddress + 1],
                                        ram[readAddress + 2], ram[readAddress + 3]} ==
            std::array<std::uint8_t, 4>{0xFA, 0xCE, 0xB0, 0x0C}));
    assert(runtime.dispatchEeIomanSyscall(testSeekId, static_cast<std::uint32_t>(fd), 2, 0, ram) == 2);
    assert(runtime.dispatchEeIomanSyscall(testReadId, static_cast<std::uint32_t>(fd),
                                         readAddress + 8, 2, ram) == 2);
    assert((std::array<std::uint8_t, 2>{ram[readAddress + 8], ram[readAddress + 9]} ==
            std::array<std::uint8_t, 2>{0xB0, 0x0C}));
    assert(runtime.dispatchEeIomanSyscall(testCloseId, static_cast<std::uint32_t>(fd), 0, 0, ram) == 0);
    assert(ioman.openFileCount() == 0);

    // The mapping layer is independent from the pointer translator; invalid EE
    // guest addresses are rejected after command resolution.
    assert(runtime.dispatchEeIomanSyscall(testOpenId, 0xFFFFFFFFu, 1, 0, ram) == -EFAULT);
    fs::remove_all(root);
    return 0;
}
