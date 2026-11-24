# Deterministic EE trace test

## Purpose

This is the first small, reproducible execution trace shared between the portable host test suite and the Xbox 360 smoke title. It is intentionally independent of retail game data: the instructions and guest RAM are created in code.

It tests the current experimental R5900 dispatcher, not the full MKSM runtime and not a PS2Recomp-generated game translation.

## Program coverage

The test executes 17 instructions from an 18-word program. A taken **BEQ** skips one non-delay instruction and executes its delay slot. The executed subset covers:

- **LUI, ORI, ADDIU, XORI and ADDU**.
- Little-endian **SW/LW** against a wrapped test RAM buffer.
- A taken **BEQ**, including normal delay-slot retirement.
- **PMTHI, PMTLO, PMFHI and PMFLO**.
- **PMADDH**, including its GPR destination and HI/LO accumulator effects.

Each operation is checked against explicit expected architectural values. The test then hashes all 32 128-bit GPRs, both HI/LO banks, PC, retired-instruction count, branch-delay state and guest data bytes at 0x80..0x93. Integer fields are fed to the hash in explicitly little-endian byte order so the signature is independent of the host's native endianness.

## Golden result

**Host CI passed**

```text
MKSM_EE_TRACE signature=4F63CD2AAFC519CE failures=00000000 pc=00000048 retired=17
```

The golden signature is enforced by the test itself. A non-zero failure mask indicates which group differs:

| Bit | Meaning |
|---:|---|
| 0 | PC, retired count or pending branch state |
| 1 | Scalar GPR results |
| 2 | PMFHI/PMFLO snapshots |
| 3 | PMADDH destination GPR |
| 4 | HI/LO accumulator values |
| 5 | Guest memory |
| 6 | Golden signature mismatch |
| 31 | Program fetch/dispatch stopped before completion |

The host test runs as part of .github/workflows/xbox360-tests.yml. The same runDeterministicTrace() function is called by port/xbox360/smoke.cpp, so the XEX writes the same MKSM_EE_TRACE line to mksm_xbox360_smoke.log at startup.

## PS2Recomp-generated execution probe

A second focused workflow, `.github/workflows/xbox360-codegen.yml`, sends a synthetic little-endian MIPS ELF through the real PS2Recomp pipeline: `PS2Recompiler::initialize/recompile/generateOutput`. The emitted C++ is compiled and executed against the desktop runtime and, separately, against the Xbox generated-runtime ABI shim on the host.

The generated function covers immediate arithmetic, ADDU, a taken BEQ and its delay slot, JR RA, and scalar memory instructions: `SB/SH/SW/SD`, `LB/LBU/LHU/LW/LD`. Its `SW` begins at `0x01FFFFFF`, testing little-endian byte order while wrapping across the 32 MiB RDRAM boundary. Both runners compare the generated register results and the memory bytes written. The XEX workflow compiles the same generated C++ for Xenon.

## Xenon generated-code ABI slice

For this first target execution, combined PS2Recomp output can be compiled with \`PS2X_GENERATED_RUNTIME_ABI\`. Its preamble includes \`ps2_generated_runtime_abi.h\` instead of the desktop SSE-based runtime headers. The shim provides a 128-bit GPR storage layout, scalar low-32/low-64 reads, sign-extending 32-bit writes, low-64 writes, and the PC/delay-slot fields touched by the probe.

The XEX workflow generates the fixture on the host first, then feeds the generated C++ source and header into the xSDK PowerPC build. The smoke title calls the generated function and logs its state. This is an intentional ABI seam for a small known subset—not a general compatibility layer. Unsupported emitted operations must remain compile failures or be added only with dedicated semantic tests.

## What this proves — and does not prove

The host test proves that the currently implemented dispatcher operations produce the documented state on the portable build. The xSDK smoke workflow proves the same source compiles into a 32-bit big-endian PowerPC XEX.

**The cross-target execution result is still pending:** the newly built XEX must be launched on an RGH/JTAG console and its signature compared with the host's 4F63CD2AAFC519CE. A successful XEX build is not proof that the probe ran on Xenon.

The generated-code workflow now has two host gates: the desktop reference runner and a runner using the same little-endian RDRAM ABI as the Xenon build. The smoke XEX compiles and runs that exact generated function during title startup. The expected console line begins `MKSM_RECOMP_XENON_TRACE failures=00000000 pc=CAFE1234 ...` and includes scalar load results. This proves only the generated scalar RDRAM slice; it does not implement the complete R5900Context, scratchpad, MMIO, 128-bit memory, MMI, VU, syscalls or runtime scheduling. A physical run is still required to establish Xenon execution parity.
