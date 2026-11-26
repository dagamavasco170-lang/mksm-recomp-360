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
    const fs::path root = fs::temp_directory_path() / "mksm_runtime_ioman_ee_pointer_test";
    fs::remove_all(root);
    fs::create_directories(root);
    {
        std::ofstream file(root / "ee_bridge.bin", std::ios::binary);
        constexpr std::array<std::uint8_t, 5> bytes{0xA1, 0xB2, 0xC3, 0xD4, 0xE5};
        file.write(reinterpret_cast<const char*>(bytes.data()), bytes.size());
    }

    ps2::xbox360::iop::IomanDispatcher dispatcher(root);
    PS2Runtime runtime;
    runtime.attachIoman(&dispatcher);
    std::vector<std::uint8_t> eeRam(ps2::xbox360::rdram_address::kRdramSize);
    constexpr std::uint32_t pathAddress = 0x00001000u;
    constexpr std::uint32_t bufferAddress = 0x00002000u;
    constexpr char guestPath[] = "ee_bridge.bin";
    for (std::size_t i = 0; i < sizeof(guestPath); ++i)
        eeRam[pathAddress + i] = static_cast<std::uint8_t>(guestPath[i]);

    const auto fd = runtime.dispatchIomanEe(IomanCommand::Open, pathAddress, 1, 0, eeRam);
    assert(fd >= ps2::xbox360::iop::RetailFileIo::FirstDescriptor);
    assert(runtime.dispatchIomanEe(IomanCommand::Read, static_cast<std::uint32_t>(fd),
                                  bufferAddress, 5, eeRam) == 5);
    assert((std::array<std::uint8_t, 5>{eeRam[bufferAddress], eeRam[bufferAddress + 1],
                                        eeRam[bufferAddress + 2], eeRam[bufferAddress + 3],
                                        eeRam[bufferAddress + 4]} ==
            std::array<std::uint8_t, 5>{0xA1, 0xB2, 0xC3, 0xD4, 0xE5}));
    assert(runtime.dispatchIomanEe(IomanCommand::Lseek, static_cast<std::uint32_t>(fd), 1, 0, eeRam) == 1);
    assert(runtime.dispatchIomanEe(IomanCommand::Read, static_cast<std::uint32_t>(fd),
                                  bufferAddress + 8, 2, eeRam) == 2);
    assert((std::array<std::uint8_t, 2>{eeRam[bufferAddress + 8], eeRam[bufferAddress + 9]} ==
            std::array<std::uint8_t, 2>{0xB2, 0xC3}));
    assert(runtime.dispatchIomanEe(IomanCommand::Close, static_cast<std::uint32_t>(fd), 0, 0, eeRam) == 0);
    assert(dispatcher.openFileCount() == 0);

    // Guest pointers outside the supported RDRAM window must fail safely.
    assert(runtime.dispatchIomanEe(IomanCommand::Open, 0xFFFFFFFFu, 1, 0, eeRam) == -EFAULT);
    fs::remove_all(root);
    return 0;
}
