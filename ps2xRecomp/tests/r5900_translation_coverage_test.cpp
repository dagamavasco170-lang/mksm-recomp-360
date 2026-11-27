#include "ps2recomp/code_generator.h"
#include "ps2recomp/instructions.h"
#include "ps2recomp/r5900_decoder.h"
#include "ps2recomp/recompiler_reporter.h"

#include <cstdint>
#include <iostream>
#include <string>
#include <vector>

namespace
{
    using namespace ps2recomp;

    uint32_t encodeI(uint32_t opcode, uint32_t rs, uint32_t rt, uint32_t immediate)
    {
        return (opcode << 26) | (rs << 21) | (rt << 16) | (immediate & 0xFFFFu);
    }

    uint32_t encodeR(uint32_t opcode, uint32_t rs, uint32_t rt, uint32_t rd,
                     uint32_t sa, uint32_t function)
    {
        return (opcode << 26) | (rs << 21) | (rt << 16) | (rd << 11) |
               (sa << 6) | (function & 0x3Fu);
    }

    bool expectSupported(CodeGenerator &generator, R5900Decoder &decoder,
                         RecompilerReporter &reporter, const char *name,
                         uint32_t raw, const char *expected)
    {
        constexpr uint32_t pc = 0x00100000u;
        const auto before = reporter.counters().unhandledInstructions;
        const Instruction inst = decoder.decodeInstruction(pc, raw, true);
        const std::string emitted = generator.translateInstruction(inst);
        const auto after = reporter.counters().unhandledInstructions;

        if (after != before)
        {
            std::cerr << "R5900_COVERAGE_FAIL supported case fell through: " << name
                      << " raw=0x" << std::hex << raw << " disasm=" << inst.disassembly
                      << " emitted=" << emitted << '\n';
            return false;
        }
        if (emitted.find(expected) == std::string::npos)
        {
            std::cerr << "R5900_COVERAGE_FAIL expected emission missing: " << name
                      << " expected=" << expected << " emitted=" << emitted << '\n';
            return false;
        }
        return true;
    }

    bool expectUnsupported(CodeGenerator &generator, R5900Decoder &decoder,
                           RecompilerReporter &reporter, const char *name,
                           uint32_t raw)
    {
        constexpr uint32_t pc = 0x00100000u;
        const auto before = reporter.counters().unhandledInstructions;
        const Instruction inst = decoder.decodeInstruction(pc, raw, true);
        const std::string emitted = generator.translateInstruction(inst);
        const auto after = reporter.counters().unhandledInstructions;

        if (after != before + 1u || emitted.find("throw std::runtime_error") == std::string::npos)
        {
            std::cerr << "R5900_COVERAGE_FAIL unsupported case was silently accepted: "
                      << name << " raw=0x" << std::hex << raw
                      << " disasm=" << inst.disassembly << " emitted=" << emitted
                      << " unhandled_delta=" << std::dec << (after - before) << '\n';
            return false;
        }
        return true;
    }
}

int main()
{
    using namespace ps2recomp;

    const std::vector<Symbol> symbols;
    const std::vector<Section> sections;
    RecompilerReporter reporter;
    CodeGenerator generator(symbols, sections);
    generator.setReporter(&reporter);
    R5900Decoder decoder;

    std::size_t supported = 0;
    auto checkSupported = [&](const char *name, uint32_t raw, const char *expected)
    {
        if (!expectSupported(generator, decoder, reporter, name, raw, expected))
            return false;
        ++supported;
        return true;
    };

    const uint32_t mfc0Status =
        encodeI(OPCODE_COP0, COP0_MF, 4, COP0_REG_STATUS);
    const uint32_t mtc0Status =
        encodeI(OPCODE_COP0, COP0_MT, 4, COP0_REG_STATUS);
    const uint32_t addSingle =
        (OPCODE_COP1 << 26) | (COP1_S << 21) | (2u << 16) |
        (1u << 11) | (3u << 6) | COP1_S_ADD;
    const uint32_t cfc2Status =
        encodeI(OPCODE_COP2, COP2_CFC2, 4, VU0_CR_VPU_STAT);
    const uint32_t vsub =
        0x4A000000u | (2u << 16) | (1u << 11) | (7u << 6) | VU0_S1_VSUB;

    const bool supportedOk =
        checkSupported("ADDIU", encodeI(OPCODE_ADDIU, 1, 2, 0x1234), "SET_GPR_S32(ctx, 2") &&
        checkSupported("DADDU", encodeR(OPCODE_SPECIAL, 1, 2, 3, 0, SPECIAL_DADDU), "SET_GPR_U64(ctx, 3") &&
        checkSupported("LQ", encodeI(OPCODE_LQ, 1, 2, 0x10), "Ps2GeneratedLq") &&
        checkSupported("MFC0_STATUS", mfc0Status, "ctx->cop0_status") &&
        checkSupported("MTC0_STATUS", mtc0Status, "ctx->cop0_status = GPR_U32") &&
        checkSupported("ADD_S", addSingle, "FPU_ADD_S") &&
        checkSupported("PSLLW", encodeR(OPCODE_MMI, 0, 2, 3, 5, MMI_PSLLW), "_mm_slli_epi32") &&
        checkSupported("CFC2_VPU_STAT", cfc2Status, "ctx->vu0_vpu_stat") &&
        checkSupported("VU_VSUB", vsub, "ps2VuSub(ctx");

    if (!supportedOk)
        return 1;

    std::size_t conservativeFallbacks = 0;
    auto checkUnsupported = [&](const char *name, uint32_t raw)
    {
        if (!expectUnsupported(generator, decoder, reporter, name, raw))
            return false;
        ++conservativeFallbacks;
        return true;
    };

    const bool unsupportedOk =
        checkUnsupported("MFC0_UNIMPLEMENTED_REGISTER",
                         encodeI(OPCODE_COP0, COP0_MF, 4, 7)) &&
        checkUnsupported("MTC0_UNIMPLEMENTED_REGISTER",
                         encodeI(OPCODE_COP0, COP0_MT, 4, 7)) &&
        checkUnsupported("CFC1_UNIMPLEMENTED_FCR",
                         encodeI(OPCODE_COP1, COP1_CF, 4, 5)) &&
        checkUnsupported("CTC1_UNIMPLEMENTED_FCR",
                         encodeI(OPCODE_COP1, COP1_CT, 4, 5)) &&
        checkUnsupported("CFC2_UNIMPLEMENTED_CONTROL",
                         encodeI(OPCODE_COP2, COP2_CFC2, 4, 19)) &&
        checkUnsupported("CTC2_UNIMPLEMENTED_CONTROL",
                         encodeI(OPCODE_COP2, COP2_CTC2, 4, 19)) &&
        checkUnsupported("LWC2_UNIMPLEMENTED",
                         encodeI(OPCODE_LWC2, 1, 2, 0)) &&
        checkUnsupported("SWC2_UNIMPLEMENTED",
                         encodeI(OPCODE_SWC2, 1, 2, 0)) &&
        checkUnsupported("COP3_UNIMPLEMENTED",
                         encodeI(OPCODE_COP3, 0, 0, 0));

    if (!unsupportedOk)
        return 1;

    std::cout << "R5900_TRANSLATION_COVERAGE_PASS supported=" << supported
              << " conservative_fallbacks=" << conservativeFallbacks << '\n';
    return 0;
}
