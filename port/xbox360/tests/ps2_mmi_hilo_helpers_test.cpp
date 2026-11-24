#include "ps2_mmi_hilo_helpers.h"

#include <cassert>
#include <cstdint>

using ps2::mmi::HiLoState;
using ps2::mmi::Vector128;

int main()
{
    HiLoState state{};
    const Vector128 initialHi{{0x10u, 0x20u, 0x30u, 0x40u}};
    const Vector128 initialLo{{0x100u, 0x200u, 0x300u, 0x400u}};

    ps2::mmi::pmthi(state, initialHi);
    ps2::mmi::pmtlo(state, initialLo);
    assert(ps2::mmi::pmfhi(state).words == initialHi.words);
    assert(ps2::mmi::pmflo(state).words == initialLo.words);

    // Eight distinct signed-halfword products land in eight independent
    // accumulator words spread over both 128-bit banks.
    const Vector128 lhs{{0x00030002u, 0x00050004u,
                         0x00070006u, 0x00090008u}};
    const Vector128 rhs{{0x0014000Au, 0x0028001Eu,
                         0x003C0032u, 0x00500046u}};
    const auto destination = ps2::mmi::pmaddh(state, lhs, rhs);

    assert(state.lo.words == std::array<std::uint32_t, 4>{
        0x00000114u, 0x0000023Cu, 0x0000042Cu, 0x000005A4u});
    assert(state.hi.words == std::array<std::uint32_t, 4>{
        0x00000088u, 0x000000E8u, 0x00000260u, 0x00000310u});
    assert(destination.words == std::array<std::uint32_t, 4>{
        0x00000114u, 0x00000088u, 0x0000042Cu, 0x00000260u});

    // The signed minimum times itself is positive 2^30. Accumulator wrap is
    // independently modulo 2^32, rather than a shared 64-bit reduction.
    HiLoState wrap{};
    wrap.lo.words = {0x7FFFFFFFu, 0xFFFFFFFFu, 0u, 0u};
    wrap.hi.words = {0xFFFFFFFFu, 0xFFFFFFFFu, 0u, 0u};
    const Vector128 minHalf{{0x80008000u, 0x80008000u,
                             0x80008000u, 0x80008000u}};
    const auto wrapped = ps2::mmi::pmaddh(wrap, minHalf, minHalf);
    assert(wrap.lo.words[0] == 0xBFFFFFFFu);
    assert(wrap.lo.words[1] == 0xBFFFFFFFu);
    assert(wrap.hi.words[0] == 0xBFFFFFFFu);
    assert(wrap.hi.words[1] == 0xBFFFFFFFu);
    assert(wrapped.words[0] == wrap.lo.words[0]);
    assert(wrapped.words[1] == wrap.hi.words[0]);
    assert(wrapped.words[2] == wrap.lo.words[2]);
    assert(wrapped.words[3] == wrap.hi.words[2]);

    // A round-trip through four 64-bit context fields preserves all 128 bits.
    std::uint64_t hiLow{}, hiHigh{}, loLow{}, loHigh{};
    ps2::mmi::to64Halves(state.hi, hiLow, hiHigh);
    ps2::mmi::to64Halves(state.lo, loLow, loHigh);
    assert(ps2::mmi::from64Halves(hiLow, hiHigh).words == state.hi.words);
    assert(ps2::mmi::from64Halves(loLow, loHigh).words == state.lo.words);
    return 0;
}
