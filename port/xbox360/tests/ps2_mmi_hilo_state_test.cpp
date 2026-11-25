#include "ps2_mmi_hilo_state.h"

#include <cassert>
#include <cstdint>

using ps2::xbox360::mmi::HiLoState;

int main()
{
    HiLoState state{};
    for (std::size_t lane = 0; lane < 4; ++lane)
    {
        state.setHiWord(lane, 0x10203040u + static_cast<std::uint32_t>(lane));
        state.setLoWord(lane, 0xA0B0C0D0u + static_cast<std::uint32_t>(lane));
    }

    for (std::size_t lane = 0; lane < 4; ++lane)
    {
        assert(state.hiWord(lane) == 0x10203040u + lane);
        assert(state.loWord(lane) == 0xA0B0C0D0u + lane);
    }

    // Lane writes must not disturb neighboring words or bank contents.
    state.setHiWord(1, 0xFFFFFFFFu);
    state.setLoWord(2, 0u);
    assert(state.hiWord(0) == 0x10203040u);
    assert(state.hiWord(1) == 0xFFFFFFFFu);
    assert(state.hiWord(2) == 0x10203042u);
    assert(state.loWord(1) == 0xA0B0C0D1u);
    assert(state.loWord(2) == 0u);
    assert(state.loWord(3) == 0xA0B0C0D3u);

    // Verify explicit PS2 guest byte order is preserved in the backing vectors.
    assert(state.hi.bytes[4] == 0xFFu && state.hi.bytes[7] == 0xFFu);
    assert(state.lo.bytes[8] == 0u && state.lo.bytes[11] == 0u);
    return 0;
}
