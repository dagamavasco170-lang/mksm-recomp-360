# Emotion Engine execution coverage and next gates

This inventory reflects the Xbox bootstrap dispatcher/executors at the time of inspection. It is a coverage map, not a claim of full PS2 CPU compatibility.

## Implemented, focused-test coverage

- Immediate operations: ADDIU, SLTI, SLTIU, ANDI, ORI, XORI, LUI.
- SPECIAL slice: logical operations, MOVZ/MOVN, basic 32-bit shifts, DSLL/DSRL/DSRA plus 32-shift and variable 64-bit shift variants (DSLLV/DSRLV/DSRAV), ADDU/SUBU, SLT/SLTU, scalar MFHI/MFLO/MTHI/MTLO transfers, MULT/MULTU and DIV/DIVU.
- Scalar HI/LO transfers copy only the low 64 bits and preserve upper 64-bit MMI state; distinct from full 128-bit MMI PMFHI/PMFLO/PMTHI/PMTLO helpers.
- Control flow: J/JAL, JR/JALR, BEQ/BNE/BLEZ/BGTZ and selected REGIMM branches with delay-slot handling; branch-in-delay control flow is deliberately rejected.
- Load/store slice: LB/LBU/LH/LHU/LW/LWU, SB/SH/SW, LWL/LWR/SWL/SWR, LQ/SQ byte transfer, plus focused generated-ABI RDRAM and scratchpad scalar probes.
- MMI: selected packed byte/halfword/word operations, HI/LO transfer and tested PMADDH/PMSUBH paths.

## Decoder/executor audit (next test gate)

A source audit confirms SPECIAL encodings for trapping ADD/SUB, SYSCALL, BREAK, and SYNC are recognized by the decoder but not implemented by the current SPECIAL executor. DSLL/DSRL/DSRA, DSLL32/DSRL32/DSRA32, and DSLLV/DSRLV/DSRAV execute the low 64-bit scalar value while preserving the upper 64 bits of the destination register. Variable shifts use GPR[rs] bits 5..0 as the shift count. MULT/MULTU and DIV/DIVU have focused implementations and tests. Existing focused tests cover selected implemented instructions and verify Unsupported is atomic for ADD and non-SPECIAL inputs. The 64-function SPECIAL audit uses an explicit implementation allowlist and checks retirement/PC behavior and selected destination/HI/LO atomicity; parameter and operand-boundary coverage remains focused rather than exhaustive.

The SPECIAL function-code audit enumerates all 64 function codes against an explicit implementation allowlist and checks retirement/PC behavior. Focused shift tests cover fixed and variable 64-bit shifts, including a shift count of 63 and preservation of destination upper64. Keep malformed/reserved encodings separate from recognized-but-unimplemented instructions.

## Multiply/divide semantics research

- MULT/MULTU are word operations with a 64-bit product split into LO/HI words. Operand restrictions and result sign-extension are EE-specific; confirm with authoritative EE references before implementation.
- DIV/DIVU produce quotient in LO and remainder in HI. Zero-divisor and signed-overflow edge cases require EE-specific verification before implementation; avoid guessing.
- Hardware multiply/divide latency and HI/LO interlocks are not modeled by this instruction-at-a-time executor. Functional tests are not cycle-accurate.

## Known gaps and risks

- COP0/COP1/COP2 are decoded into categories but not dispatched by the execution step. COP0 exception/status behavior and COP2/VU transfers may be critical.
- SPECIAL gaps include trapping ADD/SUB and exception-generating SYSCALL/BREAK; SYNC semantics still need an explicit implementation decision.
- Generated runtime ABI shim is partial: no complete R5900Context, MMIO, scheduling, IOP, VU/GS, DMAC scratchpad, full MMI, or exception/syscall implementation.
- LQ/SQ in focused dispatcher path move raw 128-bit bytes. The separate generated-code ABI probe does not establish full LQ/SQ coverage for all emitted code paths.
- Delay-slot, exception, interrupt, and privileged-state semantics remain incomplete. Unsupported instructions must fail visibly, not act as no-ops.

## Recommended priority

1. Add trapping ADD/SUB only after a coherent COP0 exception model captures Cause/EPC and branch-delay BD state.
2. Implement a defined minimal COP0 model and tests; do not fake success for unknown privileged operations.
3. Expand SPECIAL boundary tests and compare behavior against authoritative EE references.
4. Prioritize remaining COP2/VU, DMAC, GS, and runtime ABI work using generated retail-game function traces rather than synthetic instruction counts.

## Evidence boundary

CI and smoke evidence applies only to explicitly exercised probes. Host tests and PowerPC XEX compilation do not substitute for executing generated probes on physical Xbox 360 hardware. Full game execution is not established.
