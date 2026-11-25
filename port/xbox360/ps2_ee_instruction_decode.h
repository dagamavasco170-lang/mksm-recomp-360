#pragma once

#include <cstdint>

namespace ps2::xbox360::ee
{
// Decode-only R5900 instruction metadata. This deliberately does not execute
// instructions or model branch delay slots; execution semantics are a separate layer.
enum class InstructionKind : std::uint8_t
{
    Unsupported,
    Special,
    Mmi,
    Cop0,
    Cop1,
    Cop2,
    LoadStore,
    BranchImmediate
};


enum class SpecialOperation : std::uint8_t
{
    Unknown, Sll, Srl, Sra, Sllv, Srlv, Srav, Jr, Jalr, Movz, Movn,
    Syscall, Break, Sync, Mfhi, Mthi, Mflo, Mtlo, Mult, Multu, Div, Divu,
    Add, Addu, Sub, Subu, And, Or, Xor, Nor, Slt, Sltu, Dsll, Dsrl, Dsra
};
enum class MmiSubfamily : std::uint8_t { Unknown, Mmi, Mmi0, Mmi2, Mmi1, Mmi3 };

struct DecodedInstruction
{
    std::uint32_t raw{};
    std::uint8_t opcode{};
    std::uint8_t rs{};
    std::uint8_t rt{};
    std::uint8_t rd{};
    std::uint8_t sa{};
    std::uint8_t funct{};
    std::uint16_t immediate{};
    std::uint32_t target{};
    InstructionKind kind{InstructionKind::Unsupported};
    SpecialOperation special{SpecialOperation::Unknown};
    MmiSubfamily mmiSubfamily{MmiSubfamily::Unknown};

    [[nodiscard]] constexpr bool isRecognized() const noexcept
    {
        return kind != InstructionKind::Unsupported;
    }
};

[[nodiscard]] constexpr DecodedInstruction decodeInstruction(std::uint32_t word) noexcept
{
    DecodedInstruction d{};
    d.raw = word;
    d.opcode = static_cast<std::uint8_t>((word >> 26) & 0x3Fu);
    d.rs = static_cast<std::uint8_t>((word >> 21) & 0x1Fu);
    d.rt = static_cast<std::uint8_t>((word >> 16) & 0x1Fu);
    d.rd = static_cast<std::uint8_t>((word >> 11) & 0x1Fu);
    d.sa = static_cast<std::uint8_t>((word >> 6) & 0x1Fu);
    d.funct = static_cast<std::uint8_t>(word & 0x3Fu);
    d.immediate = static_cast<std::uint16_t>(word & 0xFFFFu);
    d.target = word & 0x03FFFFFFu;


    if (d.opcode == 0x00)
    {
        switch (d.funct)
        {
        case 0x00: d.special = SpecialOperation::Sll; break;
        case 0x02: d.special = SpecialOperation::Srl; break;
        case 0x03: d.special = SpecialOperation::Sra; break;
        case 0x04: d.special = SpecialOperation::Sllv; break;
        case 0x06: d.special = SpecialOperation::Srlv; break;
        case 0x07: d.special = SpecialOperation::Srav; break;
        case 0x08: d.special = SpecialOperation::Jr; break;
        case 0x09: d.special = SpecialOperation::Jalr; break;
        case 0x0A: d.special = SpecialOperation::Movz; break;
        case 0x0B: d.special = SpecialOperation::Movn; break;
        case 0x0C: d.special = SpecialOperation::Syscall; break;
        case 0x0D: d.special = SpecialOperation::Break; break;
        case 0x0F: d.special = SpecialOperation::Sync; break;
        case 0x10: d.special = SpecialOperation::Mfhi; break;
        case 0x11: d.special = SpecialOperation::Mthi; break;
        case 0x12: d.special = SpecialOperation::Mflo; break;
        case 0x13: d.special = SpecialOperation::Mtlo; break;
        case 0x18: d.special = SpecialOperation::Mult; break;
        case 0x19: d.special = SpecialOperation::Multu; break;
        case 0x1A: d.special = SpecialOperation::Div; break;
        case 0x1B: d.special = SpecialOperation::Divu; break;
        case 0x20: d.special = SpecialOperation::Add; break;
        case 0x21: d.special = SpecialOperation::Addu; break;
        case 0x22: d.special = SpecialOperation::Sub; break;
        case 0x23: d.special = SpecialOperation::Subu; break;
        case 0x24: d.special = SpecialOperation::And; break;
        case 0x25: d.special = SpecialOperation::Or; break;
        case 0x26: d.special = SpecialOperation::Xor; break;
        case 0x27: d.special = SpecialOperation::Nor; break;
        case 0x2A: d.special = SpecialOperation::Slt; break;
        case 0x2B: d.special = SpecialOperation::Sltu; break;
        case 0x38: d.special = SpecialOperation::Dsll; break;
        case 0x3A: d.special = SpecialOperation::Dsrl; break;
        case 0x3B: d.special = SpecialOperation::Dsra; break;
        default: break;
        }
    }
    if (d.opcode == 0x1C)
    {
        switch (d.funct)
        {
        case 0x00: d.mmiSubfamily = MmiSubfamily::Mmi; break;
        case 0x08: d.mmiSubfamily = MmiSubfamily::Mmi0; break;
        case 0x09: d.mmiSubfamily = MmiSubfamily::Mmi2; break;
        case 0x28: d.mmiSubfamily = MmiSubfamily::Mmi1; break;
        case 0x29: d.mmiSubfamily = MmiSubfamily::Mmi3; break;
        default: break;
        }
    }

    switch (d.opcode)
    {
    case 0x00: d.kind = InstructionKind::Special; break; // SPECIAL family
    case 0x1C: d.kind = InstructionKind::Mmi; break;     // MMI family
    case 0x10: d.kind = InstructionKind::Cop0; break;
    case 0x11: d.kind = InstructionKind::Cop1; break;
    case 0x12: d.kind = InstructionKind::Cop2; break;
    case 0x01: case 0x02: case 0x03:
    case 0x04: case 0x05: case 0x06: case 0x07:
        d.kind = InstructionKind::BranchImmediate; break;
    case 0x08: case 0x09: case 0x0A: case 0x0B:
    case 0x0C: case 0x0D: case 0x0E: case 0x0F:
    case 0x18: case 0x19: case 0x1A: case 0x1B:
    case 0x1E: case 0x1F: // LQ / SQ
    case 0x20: case 0x21: case 0x22: case 0x23:
    case 0x24: case 0x25: case 0x26: case 0x27:
    case 0x28: case 0x29: case 0x2A: case 0x2B:
    case 0x2C: case 0x2D: case 0x2E: case 0x2F:
    case 0x30: case 0x31: case 0x32: case 0x33:
    case 0x34: case 0x35: case 0x36: case 0x37:
    case 0x38: case 0x39: case 0x3A: case 0x3B:
    case 0x3C: case 0x3D: case 0x3E: case 0x3F:
        d.kind = InstructionKind::LoadStore; break;
    default: break;
    }
    return d;
}
} // namespace ps2::xbox360::ee
