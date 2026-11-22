#include "ps2_guest_memory.h"

#include <array>
#include <cassert>
#include <cstdint>
#include <span>

int main()
{
    using namespace ps2::xbox360::guest_memory;

    std::array<uint8_t, 16> ram{};
    for (size_t i = 0; i < ram.size(); ++i)
        ram[i] = static_cast<uint8_t>(i);

    uint8_t value8 = 0u;
    uint16_t value16 = 0u;
    uint32_t value32 = 0u;
    uint64_t value64 = 0u;

    assert(readLittleEndian(std::span<const uint8_t>(ram), 1u, value8));
    assert(value8 == 1u);
    assert(readLittleEndian(std::span<const uint8_t>(ram), 1u, value16));
    assert(value16 == 0x0201u);
    assert(readLittleEndian(std::span<const uint8_t>(ram), 1u, value32));
    assert(value32 == 0x04030201u);
    assert(readLittleEndian(std::span<const uint8_t>(ram), 1u, value64));
    assert(value64 == 0x0807060504030201ull);

    // Reads crossing the end of this 16-byte test RAM must wrap by its mask.
    assert(readLittleEndian(std::span<const uint8_t>(ram), 14u, value32));
    assert(value32 == 0x01000F0Eu);
    assert(readLittleEndian(std::span<const uint8_t>(ram), 12u, value64));
    assert(value64 == 0x030201000F0E0D0Cull);

    assert(writeLittleEndian(std::span<uint8_t>(ram), 15u, uint32_t{0xA1B2C3D4u}));
    assert(ram[15] == 0xD4u && ram[0] == 0xC3u &&
           ram[1] == 0xB2u && ram[2] == 0xA1u);
    assert(readLittleEndian(std::span<const uint8_t>(ram), 15u, value32));
    assert(value32 == 0xA1B2C3D4u);

    Raw128 raw{};
    assert(readRaw128(std::span<const uint8_t>(ram), 14u, raw));
    assert(raw[0] == ram[14] && raw[1] == ram[15] &&
           raw[2] == ram[0] && raw[15] == ram[13]);
    Raw128 replacement{};
    for (size_t i = 0; i < replacement.size(); ++i)
        replacement[i] = static_cast<uint8_t>(0xF0u + i);
    assert(writeRaw128(std::span<uint8_t>(ram), 15u, replacement));
    for (size_t i = 0; i < replacement.size(); ++i)
        assert(ram[(15u + i) & 15u] == replacement[i]);

    const std::array<uint8_t, 0> empty{};
    const std::array<uint8_t, 3> invalid{};
    std::array<uint8_t, 3> invalidWrite{};
    uint32_t sentinel = 0xDEADBEEFu;
    assert(!readLittleEndian(std::span<const uint8_t>(empty), 0u, sentinel));
    assert(sentinel == 0xDEADBEEFu);
    assert(!readLittleEndian(std::span<const uint8_t>(invalid), 0u, sentinel));
    assert(sentinel == 0xDEADBEEFu);
    assert(!writeLittleEndian(std::span<uint8_t>(invalidWrite), 0u, 1u));

    return 0;
}
