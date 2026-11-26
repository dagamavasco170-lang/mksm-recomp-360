#include "ps2_afs_archive.h"

#include <array>
#include <cassert>
#include <cstdint>
#include <vector>

using ps2::xbox360::disc::AfsArchive;
using ps2::xbox360::disc::AfsStatus;

static void put32(std::vector<std::uint8_t> &b, std::size_t at, std::uint32_t v)
{
    b[at] = static_cast<std::uint8_t>(v);
    b[at + 1] = static_cast<std::uint8_t>(v >> 8);
    b[at + 2] = static_cast<std::uint8_t>(v >> 16);
    b[at + 3] = static_cast<std::uint8_t>(v >> 24);
}

static std::vector<std::uint8_t> sample()
{
    std::vector<std::uint8_t> bytes(32, 0);
    bytes[0] = 'A'; bytes[1] = 'F'; bytes[2] = 'S'; bytes[3] = 0;
    put32(bytes, 4, 2);
    put32(bytes, 8, 24); put32(bytes, 12, 3);
    put32(bytes, 16, 27); put32(bytes, 20, 5);
    bytes[24] = 0xA1; bytes[25] = 0xA2; bytes[26] = 0xA3;
    bytes[27] = 0xB1; bytes[28] = 0xB2; bytes[29] = 0xB3; bytes[30] = 0xB4; bytes[31] = 0xB5;
    return bytes;
}

int main()
{
    AfsArchive archive;
    auto data = sample();
    assert(archive.open(data) == AfsStatus::Ok);
    assert(archive.isOpen());
    assert(archive.entryCount() == 2);
    assert(archive.metadata(1)->offset == 27);
    assert(archive.metadata(1)->size == 5);
    assert(archive.entry(0).size() == 3);
    assert(archive.entry(0)[0] == 0xA1 && archive.entry(0)[1] == 0xA2 && archive.entry(0)[2] == 0xA3);
    assert(archive.entry(1).size() == 5);
    assert(archive.entry(1)[0] == 0xB1 && archive.entry(1)[4] == 0xB5);
    assert(archive.entry(2).empty());

    std::array<std::uint8_t, 4> badMagic{};
    assert(archive.open(badMagic) == AfsStatus::TruncatedHeader);
    auto bad = sample();
    bad[0] = 'X';
    assert(archive.open(bad) == AfsStatus::BadMagic);
    bad = sample();
    put32(bad, 4, 99);
    assert(archive.open(bad) == AfsStatus::TruncatedTable);
    bad = sample();
    put32(bad, 8, 31);
    put32(bad, 12, 4);
    assert(archive.open(bad) == AfsStatus::InvalidEntry);
    bad = sample();
    put32(bad, 8, 24);
    put32(bad, 12, 9);
    assert(archive.open(bad) == AfsStatus::InvalidEntry);
    bad = sample();
    put32(bad, 4, AfsArchive::MaxEntries + 1);
    assert(archive.open(bad) == AfsStatus::CountTooLarge);
    return 0;
}
