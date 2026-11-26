#include "ps2_fio_ioman_adapter.h"

#include <array>
#include <cassert>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <vector>

int main()
{
    namespace fs = std::filesystem;
    using namespace ps2::xbox360::iop;
    const fs::path root = fs::temp_directory_path() / "mksm_ee_fio_ioman_adapter_test";
    fs::remove_all(root);
    fs::create_directories(root);
    {
        std::ofstream file(root / "fio_adapter.bin", std::ios::binary);
        constexpr std::array<std::uint8_t, 6> data{0x10,0x20,0x30,0x40,0x50,0x60};
        file.write(reinterpret_cast<const char*>(data.data()), data.size());
    }

    IomanDispatcher ioman(root);
    PS2Runtime runtime;
    runtime.attachIoman(&ioman);
    EeFioIomanAdapter adapter(runtime);
    std::vector<std::uint8_t> eeRam(rdram_address::kRdramSize);
    constexpr std::uint32_t pathAddr = 0x1000, bufferAddr = 0x2000;
    constexpr char path[] = "fio_adapter.bin";
    for (std::size_t i=0; i<sizeof(path); ++i)
        eeRam[pathAddr+i] = static_cast<std::uint8_t>(path[i]);

    const auto fd = adapter.open(pathAddr, EeFioIomanAdapter::FioReadOnly, eeRam);
    assert(fd >= RetailFileIo::FirstDescriptor);
    assert(adapter.open(pathAddr, EeFioIomanAdapter::FioReadWrite, eeRam) == -EACCES);
    assert(adapter.open(pathAddr, EeFioIomanAdapter::FioReadOnly | EeFioIomanAdapter::FioCreate, eeRam) == -EACCES);

    assert(adapter.read(fd, bufferAddr, 3, eeRam) == 3);
    assert((std::array<std::uint8_t,3>{eeRam[bufferAddr],eeRam[bufferAddr+1],eeRam[bufferAddr+2]} ==
            std::array<std::uint8_t,3>{0x10,0x20,0x30}));
    assert(adapter.seek(fd, 2, static_cast<std::uint32_t>(SeekWhence::Set), eeRam) == 2);
    assert(adapter.read(fd, bufferAddr+8, 2, eeRam) == 2);
    assert((std::array<std::uint8_t,2>{eeRam[bufferAddr+8],eeRam[bufferAddr+9]} ==
            std::array<std::uint8_t,2>{0x30,0x40}));
    assert(adapter.seek(fd, -1, static_cast<std::uint32_t>(SeekWhence::Set), eeRam) == -EINVAL);
    assert(adapter.seek(fd, 0, 99, eeRam) == -EINVAL);
    assert(adapter.close(fd, eeRam) == 0);
    assert(adapter.read(fd, bufferAddr, 1, eeRam) == -EBADF);
    assert(ioman.openFileCount() == 0);

    fs::remove_all(root);
    return 0;
}
