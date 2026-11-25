#include "ps2_ee_instruction_decode.h"

#include <cassert>
#include <cstdint>

using namespace ps2::xbox360::ee;

int main()
{
    // SPECIAL format field extraction.
    constexpr std::uint32_t special = (0u << 26) | (5u << 21) | (7u << 16) |
        (9u << 11) | (3u << 6) | 0x21u;
    const auto r = decodeInstruction(special);
    assert(r.raw == special);
    assert(r.kind == InstructionKind::Special);
    assert(r.rs == 5 && r.rt == 7 && r.rd == 9 && r.sa == 3 && r.funct == 0x21);

    // MMI family decode retains all fields for a future sub-decoder.
    constexpr std::uint32_t mmi = (0x1Cu << 26) | (2u << 21) | (4u << 16) |
        (6u << 11) | (1u << 6) | 0x08u;
    const auto m = decodeInstruction(mmi);
    assert(m.kind == InstructionKind::Mmi);
    assert(m.isRecognized() && m.rs == 2 && m.rt == 4 && m.rd == 6 && m.funct == 8);

    constexpr std::uint32_t cop0 = 0x40000000u;
    assert(decodeInstruction(cop0).kind == InstructionKind::Cop0);
    constexpr std::uint32_t branch = 0x10000005u;
    const auto b = decodeInstruction(branch);
    assert(b.kind == InstructionKind::BranchImmediate && b.immediate == 5);
    constexpr std::uint32_t load = 0x8C220004u; // lw rt=2, base=1, offset=4
    const auto l = decodeInstruction(load);
    assert(l.kind == InstructionKind::LoadStore && l.rs == 1 && l.rt == 2 && l.immediate == 4);
    constexpr std::uint32_t lq = (0x1Eu << 26u) | (3u << 21u) | (4u << 16u) | 0xFFF0u;
    constexpr std::uint32_t sq = (0x1Fu << 26u) | (5u << 21u) | (6u << 16u) | 0x0010u;
    const auto ql = decodeInstruction(lq);
    const auto qs = decodeInstruction(sq);
    assert(ql.kind == InstructionKind::LoadStore && ql.opcode == 0x1Eu &&
           ql.rs == 3u && ql.rt == 4u && ql.immediate == 0xFFF0u);
    assert(qs.kind == InstructionKind::LoadStore && qs.opcode == 0x1Fu &&
           qs.rs == 5u && qs.rt == 6u && qs.immediate == 0x0010u);


    // Decode selected SPECIAL functions and MMI subfamily tags.
    const auto addiu = decodeInstruction((0u << 26) | (3u << 21) | (4u << 16) | 0x21u);
    assert(addiu.kind == InstructionKind::Special && addiu.special == SpecialOperation::Addu);
    const auto mfhi = decodeInstruction(0x00000010u);
    assert(mfhi.special == SpecialOperation::Mfhi);
    const auto mmi0 = decodeInstruction(0x70000008u);
    assert(mmi0.kind == InstructionKind::Mmi && mmi0.mmiSubfamily == MmiSubfamily::Mmi0);
    const auto mmi3 = decodeInstruction(0x70000029u);
    assert(mmi3.mmiSubfamily == MmiSubfamily::Mmi3);
    const auto unknownMmi = decodeInstruction(0x7000003Fu);
    assert(unknownMmi.kind == InstructionKind::Mmi && unknownMmi.mmiSubfamily == MmiSubfamily::Unknown);

    // Unknown primary opcode must remain explicitly unsupported.
    const auto unknown = decodeInstruction(0x7C000000u);
    assert(unknown.kind == InstructionKind::Unsupported && !unknown.isRecognized());
    return 0;
}
