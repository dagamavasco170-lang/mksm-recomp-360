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

The first generated function covers immediate arithmetic, ADDU, a taken BEQ and its delay slot, JR RA, and scalar RDRAM instructions: `SB/SH/SW/SD`, `LB/LBU/LHU/LW/LD`. Its aligned `SW` targets the final aligned RDRAM word at `0x01FFFFF8`. A separate base register addresses `0x20000000`, the documented uncached RDRAM alias, so an aligned `SD/LD` at that address checks backing offset zero and byte/halfword operations verify other bytes through the same alias. The test does not treat `0x02000000` as a virtual alias and avoids architecturally unaligned word/doubleword accesses.

A second 15-instruction function in the same synthetic ELF exercises the canonical `0x70000000` scratchpad and project compatibility alias `0xF0000000` with generated `SW/LW/SB/LBU/SH/LHU/SD/LD` operations. It checks little-endian bytes, reads data through the other alias after writes, and stores a word at scratchpad offset `0x10` while verifying the same RDRAM offset remains zero. The generated memory ABI host runner executes both generated entry points and checks every result. The XEX workflow compiles both functions and the same smoke probe calls them at startup.

The address tests separately check the normal, uncached and accelerated RDRAM windows, limited KSEG aliases, special region classification and rejection of unsupported areas by the RDRAM-only resolver. The RDRAM-only API continues to reject scratchpad so callers cannot accidentally treat it as main RAM; scratchpad is routed by the guest-memory dispatcher to its own bounded per-runtime allocation.

## Xenon generated-code ABI slice

For this first target execution, combined PS2Recomp output can be compiled with \`PS2X_GENERATED_RUNTIME_ABI\`. Its preamble includes \`ps2_generated_runtime_abi.h\` instead of the desktop SSE-based runtime headers. The shim provides a 128-bit GPR storage layout, scalar low-32/low-64 reads, sign-extending 32-bit writes, low-64 writes, and the PC/delay-slot fields touched by the probe.

The XEX workflow generates the fixture on the host first, then feeds the generated C++ source and header into the xSDK PowerPC build. The smoke title calls the generated function and logs its state. This is an intentional ABI seam for a small known subset—not a general compatibility layer. Unsupported emitted operations must remain compile failures or be added only with dedicated semantic tests.

## What this proves — and does not prove

The host test proves that the currently implemented dispatcher operations produce the documented state on the portable build. The xSDK smoke workflow proves the same source compiles into a 32-bit big-endian PowerPC XEX.

**The cross-target execution result is still pending:** the newly built XEX must be launched on an RGH/JTAG console and its signature compared with the host's 4F63CD2AAFC519CE. A successful XEX build is not proof that the probe ran on Xenon.

The generated-code workflow has a desktop reference runner for the RDRAM function and a Xenon-ABI host runner that executes both generated entries. The smoke XEX compiles and calls both entries during title startup. The expected `MKSM_RECOMP_XENON_TRACE` now includes the scratchpad word/byte/halfword/doubleword results, compatibility-alias values and RDRAM-isolation word. This proves the tested scalar RDRAM and scratchpad operations on the host ABI and proves the same generated source compiles for PowerPC; it does not prove execution on physical Xenon hardware. The shim still does not implement scratchpad DMAC paths, complete R5900Context, MMIO, VU/GS/IOP, 128-bit memory, full MMI, syscalls or runtime scheduling.

**Verified CI result (2026-11-24, commit `efff5c5057f575d77150d049710cfa3288ef548a`):** the desktop reference runner and the Xenon ABI host runner both completed with `failures=00000000`. The smoke XEX was also compiled and uploaded successfully by run `45402422067` (artifact `14923867389`). Its execution on a physical RGH/JTAG console remains pending.
