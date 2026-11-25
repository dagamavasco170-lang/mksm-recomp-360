#include "ps2_ee_special_executor.h"

#include <cassert>
#include <cstdint>

using namespace ps2::xbox360::ee;

static constexpr std::uint32_t encode(std::uint8_t rs, std::uint8_t rt,
                                    std::uint8_t rd, std::uint8_t sa,
                                    std::uint8_t funct)
{
    return (static_cast<std::uint32_t>(rs) << 21) |
           (static_cast<std::uint32_t>(rt) << 16) |
           (static_cast<std::uint32_t>(rd) << 11) |
           (static_cast<std::uint32_t>(sa) << 6) | funct;
}


static constexpr bool expectedImplemented(std::uint8_t f)
{
    switch (f)
    {
    case 0x00: case 0x02: case 0x03: case 0x04: case 0x06: case 0x07:
    case 0x0A: case 0x0B: case 0x10: case 0x11: case 0x12: case 0x13:
    case 0x21: case 0x23: case 0x24: case 0x25: case 0x26: case 0x27:
    case 0x2A: case 0x2B:
        return true;
    default:
        return false;
    }
}

int main()
{
    for (std::uint8_t funct = 0; funct < 64; ++funct)
    {
        CpuState state{};
        state.pc = 0x1000;
        state.retiredInstructions = 7;
        state.gpr[1].bytes[0] = 0x13;
        state.gpr[2].bytes[0] = 0x57;
        state.gpr[3].bytes[0] = 0xA9;
        state.hilo.hi.bytes[0] = 0xC1;
        state.hilo.lo.bytes[0] = 0xD2;

        const auto decoded = decodeInstruction(encode(1, 2, 3, 1, funct));
        assert(decoded.kind == InstructionKind::Special);
        const auto status = executeSpecial(state, decoded);

        if (expectedImplemented(funct))
        {
            assert(status == ExecuteStatus::Executed);
            assert(state.pc == 0x1004);
            assert(state.retiredInstructions == 8);
        }
        else
        {
            assert(status == ExecuteStatus::Unsupported);
            assert(state.pc == 0x1000);
            assert(state.retiredInstructions == 7);
            assert(state.gpr[3].bytes[0] == 0xA9);
            assert(state.hilo.hi.bytes[0] == 0xC1);
            assert(state.hilo.lo.bytes[0] == 0xD2);
        }
    }
    return 0;
}
