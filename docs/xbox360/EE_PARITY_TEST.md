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

A second 15-instruction function exercises the canonical `0x70000000` scratchpad and project compatibility alias `0xF0000000` with generated `SW/LW/SB/LBU/SH/LHU/SD/LD` operations. It checks little-endian bytes, alias coherence and RDRAM isolation.

A third 12-instruction function exercises the MMI HI/LO path through actual PS2Recomp-generated C++: `PMTHI`, `PMTLO`, `PMFHI`, `PMFLO`, `PMADDH`, reads of the updated banks, then `PMSUBH` and a final pair of bank reads. It seeds four distinct words in each bank, verifies all eight PMADDH accumulator words plus its GPR destination, and then confirms PMSUBH restores every HI/LO word and produces the expected GPR destination. Both the desktop reference runner and the Xenon-ABI host runner execute the same generated function and return zero failures. The XEX workflow compiles the same three generated functions and the smoke title calls the combined probe at startup.

The address tests separately check the normal, uncached and accelerated RDRAM windows, limited KSEG aliases, special region classification and rejection of unsupported areas by the RDRAM-only resolver. The RDRAM-only API continues to reject scratchpad so callers cannot accidentally treat it as main RAM; scratchpad is routed by the guest-memory dispatcher to its own bounded per-runtime allocation.
## Xenon generated-code ABI slice

For the generated target probe, combined PS2Recomp output can be compiled with `PS2X_GENERATED_RUNTIME_ABI`. Its preamble includes `ps2_generated_runtime_abi.h` instead of the desktop SSE-based runtime headers. The shim provides 128-bit GPR storage, scalar low-32/low-64 accessors, sign-extending 32-bit writes, PC/delay-slot state, scalar RDRAM and scratchpad helpers, and a full-width MMI HI/LO bridge for the tested `PMFHI/PMFLO/PMTHI/PMTLO/PMADDH` operations.

The XEX workflow generates the fixture on the host first, then feeds the generated C++ source and header into the xSDK PowerPC build. The smoke title calls the generated function and logs its state. This is an intentional ABI seam for a small known subset—not a general compatibility layer. Unsupported emitted operations must remain compile failures or be added only with dedicated semantic tests.

## What this proves — and does not prove

The generated-code workflow has a desktop reference runner for RDRAM and a Xenon-ABI host runner for RDRAM, scratchpad and the selected MMI function. The smoke XEX compiles the same three generated entry points and invokes the combined probe during title startup. Its expected trace includes the scratchpad and alias values plus MMI destination and all eight HI/LO accumulator words. This proves the tested operations on both host execution paths and proves the generated source compiles for PowerPC; it does not prove execution on physical Xenon hardware. The shim still does not implement scratchpad DMAC paths, complete R5900Context, MMIO, VU/GS/IOP, 128-bit LQ/SQ memory instructions, the rest of MMI, syscalls or runtime scheduling.

**Verified CI result (2026-11-24, code commit `6ac4cc82f60771699567f12f2a6cc57ad4d3d374`):** portable tests passed in run `45436511271`; PS2Recomp desktop and Xenon-ABI generated-code tests passed in run `45436511346`; xSDK compiled and uploaded the PowerPC XEX in run `45436512067`. The MMI trace was `MKSM_RECOMP_MMI_TRACE failures=00000000 pc=CAFE5678 dest=00000114/00000088/0000042C/00000260 hi=00000088/000000E8/00000260/00000310 lo=00000114/0000023C/0000042C/000005A4 subdest=00000100/00000010/00000300/00000030 subhi=00000010/00000020/00000030/00000040 sublo=00000100/00000200/00000300/00000400`. This shows the expected PMADDH results and confirms that PMSUBH restores the initial HI/LO banks. The memory trace also ended with `mmi=00000000`. The XEX was built from this exact code head as artifact `14941382054`, SHA-256 `6df34fcc4b6f0af98ce5e223837ac4541655dba915515af7b75dae37b3199c49`. Its execution on a physical RGH/JTAG console remains pending.
