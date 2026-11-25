#include "ps2_retail_disc.h"

#include <array>
#include <cassert>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

using ps2::xbox360::disc::FileStatus;
using ps2::xbox360::disc::RetailDisc;

int main()
{
    const auto root = std::filesystem::temp_directory_path() /
                      "mksm-xbox360-retail-disc-test";
    std::error_code error;
    std::filesystem::remove_all(root, error);
    std::filesystem::create_directories(root / "IOP");

    {
        std::ofstream wad(root / "GAMEDATA.WAD", std::ios::binary);
        const std::array<std::uint8_t, 8> bytes{
            0x4Du, 0x4Bu, 0x53u, 0x4Du, 0x10u, 0x20u, 0x30u, 0x40u};
        wad.write(reinterpret_cast<const char *>(bytes.data()),
                  static_cast<std::streamsize>(bytes.size()));
        assert(wad.good());
    }
    {
        std::ofstream module(root / "IOP" / "SIO2MAN.IRX", std::ios::binary);
        module << "module";
        assert(module.good());
    }

    const RetailDisc disc(root);
    std::uint64_t size{};
    assert(disc.fileSize("GAMEDATA.WAD", size) == FileStatus::Ok && size == 8u);
    assert(disc.exists("IOP/SIO2MAN.IRX"));
    assert(disc.exists("IOP\\SIO2MAN.IRX"));
    assert(disc.exists("cdrom0:/IOP/SIO2MAN.IRX;1"));
    assert(disc.exists("CDROM0:\\\\IOP\\\\SIO2MAN.IRX;1"));
    assert(disc.exists("cdrom1:IOP/SIO2MAN.IRX"));

    std::array<std::uint8_t, 4> slice{};
    assert(disc.read("GAMEDATA.WAD", 2u, slice) == FileStatus::Ok);
    assert((slice == std::array<std::uint8_t, 4>{0x53u, 0x4Du, 0x10u, 0x20u}));

    std::vector<std::uint8_t> whole;
    assert(disc.readAll("GAMEDATA.WAD", whole) == FileStatus::Ok);
    assert(whole.size() == 8u && whole[0] == 0x4Du && whole[7] == 0x40u);

    assert(disc.read("GAMEDATA.WAD", 7u, slice) == FileStatus::OutOfRange);
    assert(disc.readAll("MISSING.BIN", whole) == FileStatus::NotFound);
    assert(disc.fileSize("../outside.bin", size) == FileStatus::InvalidPath);
    assert(disc.fileSize("IOP/../../outside.bin", size) == FileStatus::InvalidPath);
    assert(disc.fileSize("/etc/passwd", size) == FileStatus::InvalidPath);
    assert(disc.fileSize("Hdd:/Games/MKSM/GAMEDATA.WAD", size) ==
           FileStatus::InvalidPath);
    assert(disc.fileSize("", size) == FileStatus::InvalidPath);

    std::filesystem::remove_all(root, error);
    return 0;
}
