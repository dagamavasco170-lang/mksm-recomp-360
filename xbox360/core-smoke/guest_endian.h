#pragma once

#include <cstdint>

namespace ps2x::guest
{
    // PS2 EE guest memory is little-endian, regardless of the host CPU's byte order.
    // These byte-wise helpers avoid host-endian memcpy loads on Xenon (PowerPC).
    constexpr uint16_t load_u16_le(const uint8_t *p)
    {
        return static_cast<uint16_t>(p[0]) |
               (static_cast<uint16_t>(p[1]) << 8);
    }

    constexpr uint32_t load_u32_le(const uint8_t *p)
    {
        return static_cast<uint32_t>(p[0]) |
               (static_cast<uint32_t>(p[1]) << 8) |
               (static_cast<uint32_t>(p[2]) << 16) |
               (static_cast<uint32_t>(p[3]) << 24);
    }

    constexpr uint64_t load_u64_le(const uint8_t *p)
    {
        return static_cast<uint64_t>(load_u32_le(p)) |
               (static_cast<uint64_t>(load_u32_le(p + 4)) << 32);
    }

    constexpr void store_u16_le(uint8_t *p, uint16_t v)
    {
        p[0] = static_cast<uint8_t>(v);
        p[1] = static_cast<uint8_t>(v >> 8);
    }

    constexpr void store_u32_le(uint8_t *p, uint32_t v)
    {
        p[0] = static_cast<uint8_t>(v);
        p[1] = static_cast<uint8_t>(v >> 8);
        p[2] = static_cast<uint8_t>(v >> 16);
        p[3] = static_cast<uint8_t>(v >> 24);
    }

    constexpr void store_u64_le(uint8_t *p, uint64_t v)
    {
        store_u32_le(p, static_cast<uint32_t>(v));
        store_u32_le(p + 4, static_cast<uint32_t>(v >> 32));
    }
}
