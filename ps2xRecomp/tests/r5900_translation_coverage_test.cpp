#include "ps2recomp/code_generator.h"
#include "ps2recomp/instructions.h"
#include "ps2recomp/r5900_decoder.h"
#include "ps2recomp/recompiler_reporter.h"
#include "ps2recomp/types.h"

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

    uint32_t encodeCopRegister(uint32_t opcode, uint32_t format, uint32_t rt, uint32_t rd)
    {
        return (opcode << 26) | (format << 21) | (rt << 16) | (rd << 11);
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
        encodeCopRegister(OPCODE_COP0, COP0_MF, 4, COP0_REG_STATUS);
    const uint32_t mtc0Status =
        encodeCopRegister(OPCODE_COP0, COP0_MT, 4, COP0_REG_STATUS);
    const uint32_t addSingle =
        (OPCODE_COP1 << 26) | (COP1_S << 21) | (2u << 16) |
        (1u << 11) | (3u << 6) | COP1_S_ADD;
    const uint32_t cfc2Status =
        encodeCopRegister(OPCODE_COP2, COP2_CFC2, 4, VU0_CR_VPU_STAT);
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
        checkSupported("VU_VSUB", vsub, "ps2VuSub(ctx") &&
        checkSupported("ADD_OVERFLOW_GUARD", encodeR(OPCODE_SPECIAL, 1, 2, 3, 0, SPECIAL_ADD), "Ps2EeExceptionCode::IntegerOverflow") &&
        checkSupported("SUB_OVERFLOW_GUARD", encodeR(OPCODE_SPECIAL, 1, 2, 3, 0, SPECIAL_SUB), "Ps2EeExceptionCode::IntegerOverflow") &&
        checkSupported("MULT_SIGNED_HILO", encodeR(OPCODE_SPECIAL, 1, 2, 0, 0, SPECIAL_MULT), "ctx->lo") &&
        checkSupported("MULT_UNSIGNED_HILO", encodeR(OPCODE_SPECIAL, 1, 2, 0, 0, SPECIAL_MULTU), "ctx->hi") &&
        checkSupported("DIV_SIGNED_EDGE_GUARDS", encodeR(OPCODE_SPECIAL, 1, 2, 0, 0, SPECIAL_DIV), "divisor == -1 && dividend == INT32_MIN") &&
        checkSupported("DIV_UNSIGNED_ZERO_GUARD", encodeR(OPCODE_SPECIAL, 1, 2, 0, 0, SPECIAL_DIVU), "divisor != 0") &&
        checkSupported("MFHI", encodeR(OPCODE_SPECIAL, 0, 0, 3, 0, SPECIAL_MFHI), "ctx->hi") &&
        checkSupported("MFLO", encodeR(OPCODE_SPECIAL, 0, 0, 3, 0, SPECIAL_MFLO), "ctx->lo");

    if (!supportedOk)
        return 1;

    // Overflow codegen must signal before the destination write path; the
    // generated else branch is the only place the destination is committed.
    const auto addOverflowInst = decoder.decodeInstruction(
        0x00100000u, encodeR(OPCODE_SPECIAL, 1, 2, 3, 0, SPECIAL_ADD), true);
    const auto addOverflowCode = generator.translateInstruction(addOverflowInst);
    const auto addSignal = addOverflowCode.find("SignalException(ctx, Ps2EeExceptionCode::IntegerOverflow)");
    const auto addCommit = addOverflowCode.find("SET_GPR_S32(ctx, 3");
    if (addSignal == std::string::npos || addCommit == std::string::npos ||
        addSignal >= addCommit || addOverflowCode.find("else") == std::string::npos)
    {
        std::cerr << "R5900_OVERFLOW_FAIL ADD overflow guard/commit ordering: "
                  << addOverflowCode << '\n';
        return 1;
    }

    const auto subOverflowInst = decoder.decodeInstruction(
        0x00100000u, encodeR(OPCODE_SPECIAL, 1, 2, 3, 0, SPECIAL_SUB), true);
    const auto subOverflowCode = generator.translateInstruction(subOverflowInst);
    const auto subSignal = subOverflowCode.find("SignalException(ctx, Ps2EeExceptionCode::IntegerOverflow)");
    const auto subCommit = subOverflowCode.find("SET_GPR_S32(ctx, 3");
    if (subSignal == std::string::npos || subCommit == std::string::npos ||
        subSignal >= subCommit || subOverflowCode.find("SUB32_OV") == std::string::npos ||
        subOverflowCode.find("else") == std::string::npos)
    {
        std::cerr << "R5900_OVERFLOW_FAIL SUB overflow guard/commit ordering: "
                  << subOverflowCode << '\n';
        return 1;
    }
    std::cout << "R5900_OVERFLOW_EMISSION_PASS add=sub_guarded sub=sub_guarded\n";

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
                         encodeCopRegister(OPCODE_COP0, COP0_MF, 4, 7)) &&
        checkUnsupported("MTC0_UNIMPLEMENTED_REGISTER",
                         encodeCopRegister(OPCODE_COP0, COP0_MT, 4, 7)) &&
        checkUnsupported("CFC1_UNIMPLEMENTED_FCR",
                         encodeCopRegister(OPCODE_COP1, COP1_CF, 4, 5)) &&
        checkUnsupported("CTC1_UNIMPLEMENTED_FCR",
                         encodeCopRegister(OPCODE_COP1, COP1_CT, 4, 5)) &&
        checkUnsupported("CFC2_UNIMPLEMENTED_CONTROL",
                         encodeCopRegister(OPCODE_COP2, COP2_CFC2, 4, 19)) &&
        checkUnsupported("CTC2_UNIMPLEMENTED_CONTROL",
                         encodeCopRegister(OPCODE_COP2, COP2_CTC2, 4, 19)) &&
        checkUnsupported("LWC2_UNIMPLEMENTED",
                         encodeI(OPCODE_LWC2, 1, 2, 0)) &&
        checkUnsupported("SWC2_UNIMPLEMENTED",
                         encodeI(OPCODE_SWC2, 1, 2, 0)) &&
        checkUnsupported("COP0_CO_UNIMPLEMENTED",
                         encodeCopRegister(OPCODE_COP0, COP0_CO, 0, 0) | 0x3Fu) &&
        checkUnsupported("SPECIAL_UNKNOWN_FUNCTION",
                         encodeR(OPCODE_SPECIAL, 1, 2, 3, 0, 0x1Fu)) &&
        checkUnsupported("REGIMM_UNKNOWN_SUBCODE",
                         encodeI(OPCODE_REGIMM, 1, 4, 0)) &&
        checkUnsupported("MMI_UNKNOWN_FUNCTION",
                         encodeR(OPCODE_MMI, 0, 2, 3, 0, 0x3Du)) &&
        checkUnsupported("FPU_UNKNOWN_FORMAT",
                         encodeCopRegister(OPCODE_COP1, COP1_D, 4, 1)) &&
        checkUnsupported("FPU_S_UNSUPPORTED_FUNCTION",
                         (OPCODE_COP1 << 26) | (COP1_S << 21) | (2u << 16) |
                             (1u << 11) | (3u << 6) | COP1_S_ROUND_L) &&
        checkUnsupported("VU_UNKNOWN_FORMAT",
                         encodeCopRegister(OPCODE_COP2, 0x0Fu, 4, 1)) &&
        checkUnsupported("PRIMARY_OPCODE_UNIMPLEMENTED",
                         encodeI(0x3Bu, 1, 2, 0));

    if (!unsupportedOk)
        return 1;

    std::cout << "R5900_TRANSLATION_COVERAGE_PASS supported=" << supported
              << " conservative_fallbacks=" << conservativeFallbacks << '\n';
    return 0;
}
