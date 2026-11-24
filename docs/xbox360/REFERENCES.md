# Technical references and reuse boundaries

The Shaolin Monks Xbox 360 port uses two complementary public projects as engineering references. They are references for architecture, investigations and test methodology—not a substitute for implementing and validating the game's own PS2 behavior.

## Reference chain

- [Tenkaichi3Decomp](https://github.com/z3xox/Tenkaichi3Decomp): PC recompilation/decompilation-based reconstruction of Budokai Tenkaichi 3. Study how game code, platform dependencies, and generated/runtime components are organized.
- [BT3XBx](https://github.com/Lucas-Tito/bt3xbx): the closest direct precedent. It ports the PS2 release of BT3 to an RGH/JTAG `.xex`, with the Xbox target isolated under `port/x360/`, a PowerPC game build, native D3D9/XAudio2/XInput layers and a staged plan from compiler probes through headless parity to console gameplay. Study its port boundary, phase gates, reproducible builds, precise floating-point policy, diagnostic logging and real-console acceptance. Its XDK toolchain is proprietary and is not adopted here.
- [Laika64](https://github.com/ErickaguirreD/Laika64): a native Xbox 360 Nintendo 64 emulator, not a PS2 game port. Its MIPS-to-PowerPC dynarec is valuable for Xenon-specific lessons: block-cache organization and invalidation, hot/cold code layout, register caching with explicit interpreter sync points, a writable/executable `.jitc` image section, instruction-cache maintenance, and PowerPC opcode-emitter tests generated against GNU `as`. It also has host unit tests and a console lockstep test mode comparing the JIT with the interpreter. Its Xbox target uses the Visual Studio 2010/XDK project, not this repository's pinned xSDK path.
- [Ibara360](https://github.com/PaulAtreides67/ibara360): an adjacent native Xbox 360 project built with xSDK and tested on a real RGH/JTAG console. It recompiles the arcade CV1000 board's SH-3 programs and applies the PS2 port's Arrange mode as SH-3 patches; it is **not** a PS2 EE-to-Xenon port. Use it as a reference for xSDK title structure, packaging, storage/input/audio integration and hardware evidence—not as a source for PS2 CPU semantics.
- [xSDK](https://github.com/xenosworks/xsdk): the open Xbox 360 SDK already pinned by this repository. Study its title samples, XEX packaging, Xenos command path, XAM/kernel APIs and hardware probes. Keep it external and pinned; do not mix in proprietary XDK files.

## What is genuinely comparable

BT3XBx is the direct comparison because both projects start from a PlayStation 2 game and aim for a native Xbox 360 title. The implementation route differs: BT3XBx starts from a decompilation/PC port and compiles reconstructed game C/C++ for Xenon; MKSM starts from the PS2Recomp pipeline and therefore must validate generated EE code and the runtime services that generated code calls. We should copy the engineering discipline, not assume the code paths are interchangeable.

Ibara360 validates another useful path: xSDK can build native XEX titles with real console subsystems, but its main game runtime is an SH-3/CV1000 reimplementation. Its PS2 contribution is extracted data and an Arrange-mode patch on the arcade program, not a general-purpose R5900 emulator or recompiler.

Laika64 validates the feasibility of a runtime MIPS-to-Xenon dynarec, executable code-cache management and native Xenos rendering on real RGH/JTAG hardware. It does not validate PS2 EE semantics: the Nintendo 64 CPU is a different MIPS implementation, and its COP0, TLB, FPU, exceptions and memory model must not be transplanted into the R5900 core.

## How to apply lessons to MKSM

| Area | BT3XBx | Laika64 | MKSM implementation |
|---|---|---|---|
| Game/code path | PS2 decompilation compiled for Xenon | N64 MIPS code dynamically recompiled to PowerPC | Preserve PS2Recomp's host-side generation; compile translated EE code for Xenon |
| CPU/runtime | Native game/runtime boundary | Block JIT, register cache, interpreter fallback, invalidation | R5900-specific EE state and tested instruction semantics; do not reuse VR4300 behavior |
| PPC code | Compiler-generated game code | Explicit PPC emitter tested against GNU assembler | Prefer compiler-generated PPC at first; if hand-emitting/JIT is later needed, test every encoding against an assembler |
| Executable memory | Native title code | `.jitc` image section and explicit I-cache synchronization | Probe executable-code policy/cache coherency on hardware before introducing runtime code generation |
| Graphics | D3D9/Xenos integration | Xenos renderer with RDP-to-Xenos translation | GS-to-Xenos adapter with image-based tests |
| Input/audio/system | XInput/XAudio2/platform APIs | Xbox controller/audio/files/network services | Isolated xSDK/XAM adapters, only what MKSM needs |
| Validation | Headless parity, staged console acceptance | Unit tests, JIT/interpreter lockstep and real-console game cases | Deterministic desktop/PPC state traces, then hardware evidence |

## Engineering workflow

1. Inspect the relevant source and documentation in the reference project.
2. Write down the applicable concept and the exact MKSM blocker it addresses.
3. Prefer architecture and test patterns; do not transplant code blindly.
4. Verify source license, file-level notices, dependencies, and compatibility before copying or adapting any code.
5. Implement an isolated change in this repository.
6. Add game-data-free tests and host differential tests where applicable.
7. Build the Xbox target in CI; run on hardware when the acceptance criterion requires it.
8. Record exactly what the test demonstrates and what remains unverified.

## License awareness

The repository licenses are not interchangeable: Tenkaichi3Decomp is marked AGPL-3.0, BT3XBx's top-level license is MIT, and Laika64 is marked GPL-2.0. The MIT license of the Xbox port does not automatically relicense code or dependencies inherited from its upstream. Before copying or adapting source code, inspect the relevant file's history, headers, upstream origin and applicable license obligations. This document adopts these projects as research references; it does not authorize copying code or game assets.

## Current priority

The first MMI dispatcher slice (PMADDH, PMFHI, PMFLO, PMTHI and PMTLO) is connected to the experimental EE dispatcher and its portable regression test passes in CI. Keep expanding it only with instruction-manual/PCSX2/PS2Recomp cross-checks and differential vectors. The next BT3XBx-style gate is a deterministic translated EE test program whose state trace matches the desktop reference before full runtime integration. Use Laika64's lockstep and PPC-emitter test methods as patterns: compare generated state with an interpreter/reference, verify hand-emitted PPC words against an independent assembler, and prove I-cache coherency on the actual console. Those JIT-specific steps are conditional; keep our current ahead-of-time PS2Recomp/xSDK path unless profiling demonstrates a runtime JIT is needed. Then move to missing guest-memory mappings, platform services and GS-to-Xenos integration. Do not mistake a smoke XEX or host-only CPU test for game execution or console validation.
