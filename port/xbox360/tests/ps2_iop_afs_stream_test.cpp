#include "ps2_iop_afs_stream.h"

#include <array>
#include <cerrno>
#include <cassert>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <vector>

static void put32(std::vector<std::uint8_t> &b, std::size_t at, std::uint32_t v)
{
    b[at] = static_cast<std::uint8_t>(v);
    b[at + 1] = static_cast<std::uint8_t>(v >> 8);
    b[at + 2] = static_cast<std::uint8_t>(v >> 16);
    b[at + 3] = static_cast<std::uint8_t>(v >> 24);
}

int main()
{
    namespace fs = std::filesystem;
    using ps2::xbox360::iop::AfsStreamIo;
    using ps2::xbox360::iop::SeekWhence;
    const auto root = fs::temp_directory_path() / "mksm_afs_stream_test";
    fs::remove_all(root);
    fs::create_directories(root);
    std::vector<std::uint8_t> afs(40, 0);
    afs[0] = 'A'; afs[1] = 'F'; afs[2] = 'S'; afs[3] = 0;
    put32(afs, 4, 2);
    put32(afs, 8, 24); put32(afs, 12, 4);
    put32(afs, 16, 28); put32(afs, 20, 12);
    for (std::size_t i = 0; i < 16; ++i) afs[24 + i] = static_cast<std::uint8_t>(i + 10);
    {
        std::ofstream out(root / "GAMEDATA.WAG", std::ios::binary);
        out.write(reinterpret_cast<const char *>(afs.data()), static_cast<std::streamsize>(afs.size()));
    }
    AfsStreamIo io(root);
    const auto fd = io.open("afs0:/GAMEDATA.WAG/1");
    assert(fd == AfsStreamIo::FirstDescriptor);
    std::array<std::uint8_t, 5> data{};
    assert(io.read(fd, data) == 5);
    assert((data == std::array<std::uint8_t, 5>{14, 15, 16, 17, 18}));
    assert(io.seek(fd, -3, SeekWhence::End) == 9);
    std::array<std::uint8_t, 8> tail{};
    assert(io.read(fd, tail) == 3);
    assert((tail[0] == 23 && tail[1] == 24 && tail[2] == 25));
    assert(io.read(fd, tail) == 0);
    assert(io.close(fd) == 0);
    assert(io.close(fd) == -EBADF);
    assert(io.open("afs0:/GAMEDATA.WAG/2") == -EINVAL);
    assert(io.open("afs0:/GAMEDATA.WAG/nope") == -EINVAL);
    assert(io.open("../GAMEDATA.WAG/0") == -EINVAL);
    fs::remove_all(root);
    return 0;
}
