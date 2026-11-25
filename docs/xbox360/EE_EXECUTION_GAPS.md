# Emotion Engine execution coverage and next gates

This inventory reflects the Xbox bootstrap dispatcher/executors at the time of inspection. It is a coverage map, not a claim of full PS2 CPU compatibility.

## Implemented, focused-test coverage

- Immediate operations: ADDIU, SLTI, SLTIU, ANDI, ORI, XORI, LUI.
- SPECIAL slice: logical operations, MOVZ/MOVN, basic 32-bit shifts and DSLL/DSRL/DSRA plus 32-shift variants, ADDU/SUBU, SLT/SLTU, scalar MFHI/MFLO/MTHI/MTLO transfers, MULT/MULTU and DIV/DIVU.
- Scalar HI/LO transfers copy only the low 64 bits and preserve upper 64-bit MMI state; distinct from full 128-bit MMI PMFHI/PMFLO/PMTHI/PMTLO helpers.
- Control flow: J/JAL, JR/JALR, BEQ/BNE/BLEZ/BGTZ and selected REGIMM branches with delay-slot handling; branch-in-delay control flow is deliberately rejected.
- Load/store slice: LB/LBU/LH/LHU/LW/LWU, SB/SH/SW, LWL/LWR/SWL/SWR, LQ/SQ byte transfer, plus focused generated-ABI RDRAM and scratchpad scalar probes.
- MMI: selected packed byte/halfword/word operations, HI/LO transfer and tested PMADDH/PMSUBH paths.

## Decoder/executor audit (next test gate)

A source audit confirms SPECIAL encodings for trapping ADD/SUB, SYSCALL, BREAK, and SYNC are recognized by the decoder but not implemented by the current SPECIAL executor. DSLL/DSRL/DSRA and DSLL32/DSRL32/DSRA32 now execute the low 64-bit scalar value while preserving the upper 64 bits of the destination register. MULT/MULTU and DIV/DIVU have focused implementations and tests. Existing focused tests cover selected implemented instructions and verify Unsupported is atomic for ADD and non-SPECIAL inputs. The 64-function SPECIAL audit uses an explicit implementation allowlist and checks retirement/PC behavior and selected destination/HI/LO atomicity; parameter and operand-boundary coverage remains focused rather than exhaustive.

The next test addition should enumerate the 64 SPECIAL function codes (with valid operand fields), compare decoded operations against an explicit implemented-operation allowlist, and assert executor behavior: implemented entries retire exactly once and advance PC; gaps return Unsupported with PC, retirement count, and destination/HI/LO state unchanged. Keep malformed/reserved encodings separate from recognized-but-unimplemented instructions.

## Multiply/divide semantics research

- MULT/MULTU are word operations with a 64-bit product split into LO/HI words. Operand restrictions and result sign-extension are EE-specific; confirm with authoritative EE references before implementation.
- DIV/DIVU produce quotient in LO and remainder in HI. Zero-divisor and signed-overflow edge cases require EE-specific verification before implementation; avoid guessing.
- Hardware multiply/divide latency and HI/LO interlocks are not modeled by this instruction-at-a-time executor. Functional tests are not cycle-accurate.

## Known gaps and risks

- COP0/COP1/COP2 are decoded into categories but not dispatched by the execution step. COP0 exception/status behavior and COP2/VU transfers may be critical.
- SPECIAL gaps include trapping ADD/SUB, SYSCALL/BREAK/SYNC, and 64-bit shifts.
- Generated runtime ABI shim is partial: no complete R5900Context, MMIO, scheduling, IOP, VU/GS, DMAC scratchpad, full MMI, or exception/syscall implementation.
- LQ/SQ in focused dispatcher path move raw 128-bit bytes. The separate generated-code ABI probe does not establish full LQ/SQ coverage for all emitted code paths.
- Delay-slot, exception, interrupt, and privileged-state semantics remain incomplete. Unsupported instructions must fail visibly, not act as no-ops.

## Recommended priority

1. Add exhaustive SPECIAL decoder/executor contract tests as described above.
2. Implement MULT/MULTU only after operand restrictions and scalar HI/LO layout are proven; add signed/unsigned, negative, boundary, and preservation tests.
3. Implement DIV/DIVU with verified EE signedness and zero/overflow behavior.
4. Add trapping ADD/SUB and overflow tests before enabling them in game code.
5. Implement a defined minimal COP0 model and tests; do not fake success for unknown privileged operations.
6. Prioritize using generated retail-game function traces rather than synthetic instruction counts.

## Evidence boundary

CI and smoke evidence applies only to explicitly exercised probes. Host tests and PowerPC XEX compilation do not substitute for executing generated probes on physical Xbox 360 hardware. Full game execution is not established.
