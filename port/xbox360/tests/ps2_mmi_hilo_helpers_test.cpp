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

    assert((state.lo.words == std::array<std::uint32_t, 4>{
        0x00000114u, 0x0000023Cu, 0x0000042Cu, 0x000005A4u}));
    assert((state.hi.words == std::array<std::uint32_t, 4>{
        0x00000088u, 0x000000E8u, 0x00000260u, 0x00000310u}));
    assert((destination.words == std::array<std::uint32_t, 4>{
        0x00000114u, 0x00000088u, 0x0000042Cu, 0x00000260u}));

    const auto subDestination = ps2::mmi::pmsubh(state, lhs, rhs);
    assert((state.lo.words == std::array<std::uint32_t, 4>{
        0x00000100u, 0x00000200u, 0x00000300u, 0x00000400u}));
    assert((state.hi.words == std::array<std::uint32_t, 4>{
        0x00000010u, 0x00000020u, 0x00000030u, 0x00000040u}));
    assert((subDestination.words == std::array<std::uint32_t, 4>{
        0x00000100u, 0x00000010u, 0x00000300u, 0x00000030u}));

    // The signed minimum times itself is positive 2^30. Accumulator wrap is
    // independently modulo 2^32, rather than a shared 64-bit reduction.
    HiLoState wrap{};
    wrap.lo.words = {0x7FFFFFFFu, 0xFFFFFFFFu, 0u, 0u};
    wrap.hi.words = {0xFFFFFFFFu, 0xFFFFFFFFu, 0u, 0u};
    const Vector128 minHalf{{0x80008000u, 0x80008000u,
                             0x80008000u, 0x80008000u}};
    const auto wrapped = ps2::mmi::pmaddh(wrap, minHalf, minHalf);
    assert(wrap.lo.words[0] == 0xBFFFFFFFu);
    assert(wrap.lo.words[1] == 0x3FFFFFFFu);
    assert(wrap.hi.words[0] == 0x3FFFFFFFu);
    assert(wrap.hi.words[1] == 0x3FFFFFFFu);
    assert(wrapped.words[0] == wrap.lo.words[0]);
    assert(wrapped.words[1] == wrap.hi.words[0]);
    assert(wrapped.words[2] == wrap.lo.words[2]);
    assert(wrapped.words[3] == wrap.hi.words[2]);

    (void)ps2::mmi::pmsubh(wrap, minHalf, minHalf);
    assert(wrap.lo.words[0] == 0x7FFFFFFFu);
    assert(wrap.lo.words[1] == 0xFFFFFFFFu);
    assert(wrap.hi.words[0] == 0xFFFFFFFFu);
    assert(wrap.hi.words[1] == 0xFFFFFFFFu);
    assert(wrap.lo.words[2] == 0u && wrap.lo.words[3] == 0u);
    assert(wrap.hi.words[2] == 0u && wrap.hi.words[3] == 0u);

    const auto restoredPmfhl = ps2::mmi::pmfhlLw(state);
    assert((restoredPmfhl.words == std::array<std::uint32_t, 4>{
        0x00000100u, 0x00000010u, 0x00000300u, 0x00000030u}));

    const Vector128 partialSource{{0xDEADBEEFu, 0xCAFEBABEu,
                                   0x0BADF00Du, 0x8BADF00Du}};
    ps2::mmi::pmthlLw(state, partialSource);
    assert((state.lo.words == std::array<std::uint32_t, 4>{
        0xDEADBEEFu, 0x00000200u, 0x0BADF00Du, 0x00000400u}));
    assert((state.hi.words == std::array<std::uint32_t, 4>{
        0xCAFEBABEu, 0x00000020u, 0x8BADF00Du, 0x00000040u}));

    // PMFHL.UW selects the upper words in the LO/HI pair ordering.
    HiLoState pmfhlState{};
    pmfhlState.lo.words = {0x11111111u, 0x22222222u, 0x33333333u, 0x44444444u};
    pmfhlState.hi.words = {0xAAAAAAAAu, 0xBBBBBBBBu, 0xCCCCCCCCu, 0xDDDDDDDDu};
    assert((ps2::mmi::pmfhlUw(pmfhlState).words == std::array<std::uint32_t, 4>{
        0x22222222u, 0xBBBBBBBBu, 0x44444444u, 0xDDDDDDDDu}));

    // PMFHL.LH selects the halfwords in the manual's LO/HI interleave.
    pmfhlState.lo.words = {0xAABBCCDDu, 0x11223344u, 0x55667788u, 0x99AABBCCu};
    pmfhlState.hi.words = {0x12345678u, 0x23456789u, 0x3456789Au, 0x456789ABu};
    assert((ps2::mmi::pmfhlLh(pmfhlState).words == std::array<std::uint32_t, 4>{
        0x1122CCDDu, 0x23455678u, 0x99AA7788u, 0x4567789Au}));

    // PMFHL.SH saturates eight signed words to signed halfwords.
    pmfhlState.lo.words = {0x00008000u, 0xFFFF8000u, 0x80000000u, 0x7FFFFFFFu};
    pmfhlState.hi.words = {0x7FFF0000u, 0x0000FFFFu, 0x00000001u, 0xFFFFFFFFu};
    assert((ps2::mmi::pmfhlSh(pmfhlState).words == std::array<std::uint32_t, 4>{
        0x80007FFFu, 0x7FFF7FFFu, 0x7FFF8000u, 0xFFFF0001u}));

    // PMFHL.SLW clamps signed 64-bit HI||LO pairs to signed 32-bit words
    // and sign-extends them into the two 64-bit destination lanes.
    pmfhlState.lo.words = {0xFFFFFFFFu, 0u, 0x00000001u, 0u};
    pmfhlState.hi.words = {0x7FFFFFFFu, 0u, 0x80000000u, 0u};
    assert((ps2::mmi::pmfhlSlw(pmfhlState).words == std::array<std::uint32_t, 4>{
        0x7FFFFFFFu, 0u, 0x80000000u, 0xFFFFFFFFu}));

    // A round-trip through four 64-bit context fields preserves all 128 bits.
    std::uint64_t hiLow{}, hiHigh{}, loLow{}, loHigh{};
    ps2::mmi::to64Halves(state.hi, hiLow, hiHigh);
    ps2::mmi::to64Halves(state.lo, loLow, loHigh);
    assert(ps2::mmi::from64Halves(hiLow, hiHigh).words == state.hi.words);
    assert(ps2::mmi::from64Halves(loLow, loHigh).words == state.lo.words);
    return 0;
}
