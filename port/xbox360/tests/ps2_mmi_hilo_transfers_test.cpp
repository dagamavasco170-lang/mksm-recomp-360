#include "ps2_mmi_hilo_transfers.h"

#include <cassert>
#include <cstdint>

using namespace ps2::xbox360::simd;
using namespace ps2::xbox360::mmi;

int main()
{
    HiLoState state{};
    Vector128 gprHi{}, gprLo{};
    for (std::size_t i = 0; i < 16; ++i)
    {
        gprHi.bytes[i] = static_cast<std::uint8_t>(i * 7u + 3u);
        gprLo.bytes[i] = static_cast<std::uint8_t>(0xF0u - i);
    }

    // Manual-named instruction aliases must preserve all 128 bits.
    pmthi(state, gprHi);
    pmtlo(state, gprLo);
    assert(pmfhi(state).bytes == gprHi.bytes);
    assert(pmflo(state).bytes == gprLo.bytes);

    // Replacing HI must not alter LO, and vice versa.
    Vector128 replacementHi{}, replacementLo{};
    for (std::size_t i = 0; i < 16; ++i)
    {
        replacementHi.bytes[i] = static_cast<std::uint8_t>(0xA5u ^ i);
        replacementLo.bytes[i] = static_cast<std::uint8_t>(0x5Au + i);
    }
    pmthi(state, replacementHi);
    assert(pmfhi(state).bytes == replacementHi.bytes);
    assert(pmflo(state).bytes == gprLo.bytes);

    pmtlo(state, replacementLo);
    assert(pmflo(state).bytes == replacementLo.bytes);
    assert(pmfhi(state).bytes == replacementHi.bytes);
    return 0;
}
