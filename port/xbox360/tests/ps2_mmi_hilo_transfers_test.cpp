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

    mthi(state, gprHi);
    mtlo(state, gprLo);
    assert(mfhi(state).bytes == gprHi.bytes);
    assert(mflo(state).bytes == gprLo.bytes);

    // Writes and reads are full-width and bank-independent.
    Vector128 replacement{};
    for (std::size_t i = 0; i < 16; ++i)
        replacement.bytes[i] = static_cast<std::uint8_t>(0xA5u ^ i);
    mthi(state, replacement);
    assert(mfhi(state).bytes == replacement.bytes);
    assert(mflo(state).bytes == gprLo.bytes);

    mtlo(state, replacement);
    assert(mflo(state).bytes == replacement.bytes);
    assert(mfhi(state).bytes == replacement.bytes);
    return 0;
}
