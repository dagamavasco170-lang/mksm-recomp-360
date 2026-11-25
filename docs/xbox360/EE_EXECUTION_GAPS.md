# Emotion Engine execution coverage and next gates

This inventory reflects the Xbox bootstrap dispatcher/executors at the time of inspection. It is a coverage map, not a claim of full PS2 CPU compatibility.

## Implemented, focused-test coverage

- Immediate operations: ADDIU, SLTI, SLTIU, ANDI, ORI, XORI, LUI.
- SPECIAL slice: logical operations, MOVZ/MOVN, basic 32-bit shifts, ADDU/SUBU, SLT/SLTU, and scalar MFHI/MFLO/MTHI/MTLO transfers.
- Scalar HI/LO transfers copy only the low 64 bits and preserve upper 64-bit MMI state; this is distinct from the full 128-bit MMI PMFHI/PMFLO/PMTHI/PMTLO helpers.
- Control flow: J/JAL, JR/JALR, BEQ/BNE/BLEZ/BGTZ and selected REGIMM branches with delay-slot handling; branch-in-delay control flow is deliberately rejected.
- Load/store slice: LB/LBU/LH/LHU/LW/LWU, SB/SH/SW, LWL/LWR/SWL/SWR, LQ/SQ byte transfer, plus focused generated-ABI RDRAM and scratchpad scalar probes.
- MMI: selected packed byte/halfword/word operations, HI/LO transfer and tested PMADDH/PMSUBH paths.

## SPECIAL multiply/divide semantics research

- EE MULT and MULTU are 32-bit word operations whose 64-bit product is split into 32-bit LO and HI words. The EE manual requires source operands to be sign-extended 32-bit word values; otherwise the result is undefined. The 32-bit result halves are sign-extended into the scalar 64-bit HI/LO views. Do not treat these as 128-bit multiplies.
- DIV and DIVU return a 32-bit quotient in LO and a 32-bit remainder in HI. Zero-divisor and signed-overflow cases require EE-specific behavior to be verified and tested before implementation; reference implementations disagree on some upper-word extension details.
- EE multiply/divide operations are asynchronous on hardware, with HI/LO interlocks. The current portable executor is instruction-at-a-time and does not model latency. This is acceptable only as an explicitly documented functional approximation; do not claim cycle accuracy.

## Known gaps and risks

- COP0/COP1/COP2 are decoded into categories but are not dispatched by the execution step. The game may depend on COP0 status/exception behavior and COP2/VU transfers.
- SPECIAL decode recognizes operations not yet implemented by the executor: trapping ADD/SUB, MULT/MULTU/DIV/DIVU, SYSCALL/BREAK/SYNC, and DSLL/DSRL/DSRA variants.
- The generated runtime ABI shim is intentionally partial. It does not implement complete R5900Context, MMIO, scheduling, IOP, VU/GS, DMAC scratchpad behavior, full MMI, or exception/syscall handling.
- LQ/SQ in the focused dispatcher path move raw 128-bit bytes. The separate generated-code ABI probe does not establish full LQ/SQ support for all emitted code paths.
- Delay-slot, exception, interrupt, and privileged-state semantics remain incomplete. Unsupported instructions must fail visibly rather than silently execute as no-ops.

## Recommended priority

1. Add deterministic instruction-coverage tests that enumerate decoder-recognized SPECIAL encodings and assert implemented instructions execute while known gaps return Unsupported. This prevents decode/execution mismatch.
2. Implement MULT/MULTU with the documented word operand restrictions and scalar 64-bit HI/LO layout. Keep zero-divisor and signed-overflow DIV/DIVU cases gated on EE-specific verification.
3. Implement DIV/DIVU with the verified EE edge cases and signedness tests.
4. Implement 64-bit SPECIAL shifts and add edge-case tests (shift boundaries, upper/lower halves, sign behavior).
5. Add trapping ADD/SUB and overflow tests before enabling them in a game path.
6. Implement COP0 only with a defined minimal register/exception model and dedicated tests; avoid fake success for unknown privileged operations.
7. Use generated retail-game function traces to prioritize the next instructions. Do not expand solely based on synthetic instruction counts.

## Evidence boundary

The documented CI and smoke evidence applies only to the explicitly exercised probes. Host tests and PowerPC XEX compilation are not a substitute for executing the generated probe on physical Xbox 360 hardware. Full game execution is not established.
