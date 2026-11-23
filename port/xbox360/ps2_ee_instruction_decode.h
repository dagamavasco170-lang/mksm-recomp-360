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
