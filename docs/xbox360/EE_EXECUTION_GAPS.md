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

- MULT/MULTU and DIV/DIVU have focused functional implementations and host tests. The scalar executor accepts sign-extended 32-bit GPR operands and preserves the upper 64-bit MMI portions of HI/LO.
- DIV/DIVU place quotient in LO and remainder in HI. A zero divisor is rejected as Unsupported without mutating state because the result is architecturally undefined. Signed INT32_MIN / -1 is handled explicitly to avoid C++ signed-overflow undefined behavior; hardware latency and interlocks are not modeled.
- Hardware multiply/divide latency and HI/LO interlocks are not modeled by this instruction-at-a-time executor. Functional tests are not cycle-accurate.

## Known gaps and risks

- COP0 subset now dispatches MFC0/MTC0 for Status, Cause (software-interrupt writable bits only), and EPC; MFC0 sign-extends its 32-bit word. ERET requires EXL, clears EXL, restores PC from EPC, and cancels pending branch-delay state. Other COP0 registers/privileged semantics remain incomplete. A minimal COP0 exception state now records Status, Cause, EPC, and BadVAddr, with EXL/BEV handling and the general level-1 vector; TLB, interrupt, coprocessor-unusable, and level-2 exception paths remain unimplemented.
- SPECIAL exception slice: ADD/SUB overflow, SYSCALL, and BREAK enter the EE level-1 general exception path; SYNC semantics still need an explicit implementation decision.
- Generated runtime ABI shim is partial: no complete R5900Context, MMIO, scheduling, IOP, VU/GS, DMAC scratchpad, full MMI, or exception/syscall implementation.
- LQ/SQ in focused dispatcher path move raw 128-bit bytes. The separate generated-code ABI probe does not establish full LQ/SQ coverage for all emitted code paths.
- Delay-slot branch-in-branch behavior, interrupt delivery, TLB, and broader privileged-state semantics remain incomplete. Unsupported instructions must fail visibly, not act as no-ops.

## Recommended priority

1. Expand COP0 register coverage and verify privilege/Status-stack semantics against EE documentation; this initial MFC0/MTC0/ERET subset has focused tests but no hardware validation.
2. Extend the exception path to address errors and coprocessor unusable only with verified EE behavior.
3. Prioritize remaining COP2/VU, DMAC, GS, and runtime ABI work using generated retail-game function traces rather than synthetic instruction counts.

## Evidence boundary

CI and smoke evidence applies only to explicitly exercised probes. Host tests and PowerPC XEX compilation do not substitute for executing generated probes on physical Xbox 360 hardware. Full game execution is not established.
