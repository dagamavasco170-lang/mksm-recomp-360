#include "ps2_generated_runtime_abi.h"

#include <array>
#include <cassert>
#include <cstdint>
#include <filesystem>
#include <fstream>

int main()
{
    namespace fs = std::filesystem;
    using ps2::xbox360::iop::IomanCommand;
    const fs::path root = fs::temp_directory_path() / "mksm_runtime_ioman_bridge_test";
    fs::remove_all(root);
    fs::create_directories(root);
    {
        std::ofstream file(root / "bridge.bin", std::ios::binary);
        constexpr std::array<std::uint8_t, 4> bytes{0x11, 0x22, 0x33, 0x44};
        file.write(reinterpret_cast<const char*>(bytes.data()), bytes.size());
    }

    ps2::xbox360::iop::IomanDispatcher dispatcher(root);
    PS2Runtime runtime;
    assert(runtime.dispatchIoman(IomanCommand::Open, 8, 1, 0, {}) == -ENOSYS);
    runtime.attachIoman(&dispatcher);
    std::array<std::uint8_t, 64> iopRam{};
    constexpr char path[] = "bridge.bin";
    for (std::size_t i = 0; i < sizeof(path); ++i)
        iopRam[8 + i] = static_cast<std::uint8_t>(path[i]);

    const auto fd = runtime.dispatchIoman(IomanCommand::Open, 8, 1, 0, iopRam);
    assert(fd >= ps2::xbox360::iop::RetailFileIo::FirstDescriptor);
    assert(runtime.dispatchIoman(IomanCommand::Read, static_cast<std::uint32_t>(fd), 32, 4, iopRam) == 4);
    assert((std::array<std::uint8_t, 4>{iopRam[32], iopRam[33], iopRam[34], iopRam[35]} ==
            std::array<std::uint8_t, 4>{0x11, 0x22, 0x33, 0x44}));
    assert(runtime.dispatchIoman(IomanCommand::Lseek, static_cast<std::uint32_t>(fd), 0, 0, iopRam) == 0);
    assert(runtime.dispatchIoman(IomanCommand::Read, static_cast<std::uint32_t>(fd), 40, 4, iopRam) == 4);
    assert(runtime.dispatchIoman(IomanCommand::Close, static_cast<std::uint32_t>(fd), 0, 0, iopRam) == 0);
    assert(dispatcher.openFileCount() == 0);
    fs::remove_all(root);
    return 0;
}
