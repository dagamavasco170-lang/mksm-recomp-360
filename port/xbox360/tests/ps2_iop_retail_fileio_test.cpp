#include "ps2_iop_retail_fileio.h"

#include <array>
#include <cassert>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>

int main()
{
    namespace fs = std::filesystem;
    const fs::path root = fs::temp_directory_path() / "mksm_iop_fileio_test";
    fs::remove_all(root);
    fs::create_directories(root / "IOP");

    {
        std::ofstream file(root / "IOP" / "MODULE.IRX", std::ios::binary);
        const std::array<std::uint8_t, 6> bytes{0x10, 0x20, 0x30, 0x40, 0x50, 0x60};
        file.write(reinterpret_cast<const char *>(bytes.data()),
                   static_cast<std::streamsize>(bytes.size()));
    }

    ps2::xbox360::iop::RetailFileIo io(root);
    using ps2::xbox360::iop::SeekWhence;

    const auto fd = io.sceOpen("IOP/MODULE.IRX");
    assert(fd == ps2::xbox360::iop::RetailFileIo::FirstDescriptor);
    assert(io.openFileCount() == 1);

    std::array<std::uint8_t, 4> first{};
    assert(io.sceRead(fd, first) == 4);
    assert((first == std::array<std::uint8_t, 4>{0x10, 0x20, 0x30, 0x40}));

    assert(io.sceLseek(fd, -2, SeekWhence::Current) == 2);
    std::array<std::uint8_t, 8> tail{};
    assert(io.sceRead(fd, tail) == 4);
    assert((tail[0] == 0x30 && tail[1] == 0x40 &&
            tail[2] == 0x50 && tail[3] == 0x60));
    assert(io.sceRead(fd, tail) == 0);

    assert(io.sceLseek(fd, -2, SeekWhence::End) == 4);
    std::array<std::uint8_t, 2> last{};
    assert(io.sceRead(fd, last) == 2);
    assert((last == std::array<std::uint8_t, 2>{0x50, 0x60}));

    assert(io.sceLseek(fd, -7, SeekWhence::Set) == -EINVAL);
    assert(io.sceLseek(fd, 0, static_cast<SeekWhence>(99)) == -EINVAL);
    assert(io.sceClose(fd) == 0);
    assert(io.sceClose(fd) == -EBADF);
    assert(io.sceRead(fd, last) == -EBADF);
    assert(io.openFileCount() == 0);

    assert(io.sceOpen("missing.bin") == -ENOENT);
    assert(io.sceOpen("../outside.bin") == -EINVAL);
    assert(io.sceOpen("IOP/MODULE.IRX", 1) == -EACCES);

    io.reset();
    fs::remove_all(root);
    return 0;
}
