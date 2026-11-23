# PS2 MMI HI/LO transfer semantics

## Verified transfer operations

The EE Core Instruction Set Manual describes these four MMI operations as full 128-bit copies:

- **PMFHI rd**: copy HI[127:0] into GPR[rd].
- **PMFLO rd**: copy LO[127:0] into GPR[rd].
- **PMTHI rs**: copy GPR[rs][127:0] into HI[127:0].
- **PMTLO rs**: copy GPR[rs][127:0] into LO[127:0].

These are distinct 128-bit banks; writing one bank must not change the other.

Reference: [EE Core Instruction Set Manual, PMFHI/PMFLO/PMTHI/PMTLO sections](https://docs.alexrp.com/mips/ee_insns.pdf) (also mirrored at [studylib](https://studylib.net/doc/28989088/ee-insns)).

## Current implementation

`ps2_mmi_hilo_state.h` provides portable bank storage.
`ps2_mmi_hilo_transfers.h` contains transfer-only helpers and manual-named aliases.
Host tests verify byte-exact 128-bit copies and HI/LO independence.

**Not yet integrated:** instruction decoder, CPU execution context, arithmetic MMI operations, timing/interlocks, or game code. Passing the host test is not evidence that a game can run.
