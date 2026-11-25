#include "guest_endian.h"

#include <array>
#include <cstdio>
#include <cstdint>

namespace
{
    int failures = 0;

    void check(bool ok, const char *label)
    {
        if (ok)
            std::printf("[PASS] %s\n", label);
        else
        {
            std::printf("[FAIL] %s\n", label);
            ++failures;
        }
    }
}

int main()
{
    std::array<uint8_t, 8> bytes{};

    ps2x::guest::store_u16_le(bytes.data(), 0xA1B2u);
    check(bytes[0] == 0xB2 && bytes[1] == 0xA1 &&
          ps2x::guest::load_u16_le(bytes.data()) == 0xA1B2u,
          "16-bit little-endian byte layout and round-trip");

    ps2x::guest::store_u32_le(bytes.data(), 0x12345678u);
    check(bytes[0] == 0x78 && bytes[1] == 0x56 &&
          bytes[2] == 0x34 && bytes[3] == 0x12 &&
          ps2x::guest::load_u32_le(bytes.data()) == 0x12345678u,
          "32-bit little-endian byte layout and round-trip");

    ps2x::guest::store_u64_le(bytes.data(), 0x0123456789ABCDEFULL);
    check(bytes[0] == 0xEF && bytes[1] == 0xCD &&
          bytes[2] == 0xAB && bytes[3] == 0x89 &&
          bytes[4] == 0x67 && bytes[5] == 0x45 &&
          bytes[6] == 0x23 && bytes[7] == 0x01 &&
          ps2x::guest::load_u64_le(bytes.data()) == 0x0123456789ABCDEFULL,
          "64-bit little-endian byte layout and round-trip");

    // Test an unaligned guest-memory address, which is valid for these helpers.
    ps2x::guest::store_u32_le(bytes.data() + 1, 0xDEADBEEFu);
    check(ps2x::guest::load_u32_le(bytes.data() + 1) == 0xDEADBEEFu,
          "unaligned 32-bit access");

    return failures == 0 ? 0 : 1;
}
