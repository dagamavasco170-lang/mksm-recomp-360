# Xbox 360 native port plan

## Goal

Produce an **Xbox 360-native homebrew build** of Mortal Kombat: Shaolin Monks for RGH/JTAG consoles. The intended deliverable is an Xbox 360 title package containing `default.xex`, launchable from Aurora.

This follows the broad engineering method demonstrated by the native BT3 Xbox 360 port: recompile the original game's program for the Xbox 360 PowerPC CPU and provide Xbox-specific graphics, audio, input, memory, file and system layers. It is **not** a PS2 emulator packaged for Xbox 360.

This document is a plan and a record of verified constraints. It does not claim that a Shaolin Monks Xbox 360 game executable exists yet.

## Latest smoke result (2026-11-22)

The run #80 smoke XEX was run on the physical console. The uploaded log records the 32-bit big-endian PowerPC ABI check, C++20 runtime checks, the little-endian guest-memory probe, scalar PS2 packed-vector probe, 1280×720 display initialization, XAM controller initialization, Xbox Back received as PS2 Select, 439 presented frames and clean shutdown with exit code 0. The log does not identify the console model/revision or dashboard/kernel version.

This validates the bootstrap title and the explicitly exercised scalar SIMD slice only. It draws a solid background and contains no PS2 EE game execution, PS2 GS rendering, audio, saves or gameplay. See [the hardware report](HARDWARE_SMOKE_2026-11-22.md).

## Source baseline and constraints

See [the initial source/build-graph audit](SOURCE_AUDIT.md) for verified CMake targets, language requirements and current platform dependencies. The PowerPC-specific SIMD blocker is documented in [SIMD_COMPATIBILITY.md](SIMD_COMPATIBILITY.md), and the first endian-safe RDRAM helper is described in [GUEST_MEMORY.md](GUEST_MEMORY.md).

- Base project: this repository's PS2 recompilation of the NTSC-U retail release, executable `SLUS_210.87`.
- Required retail executable SHA-256: `b1ed99d81b07969553ed2f9afaad4caed1bbeb637d53a369f4dafca8eedd2ab2`.
- Current public runtime status is still experimental: the opening sequence can reach scene drawing, but first-stage geometry/camera output is badly corrupted and controllable gameplay has not been established. See [STATUS.md](../STATUS.md).
- The desktop runtime currently uses C++20 and desktop-oriented dependencies, including raylib. Those assumptions must not leak into the Xbox target.
- Preferred target SDK: [xenosworks/xsdk](https://github.com/xenosworks/xsdk), evaluated in [SDK_EVALUATION.md](SDK_EVALUATION.md). Its GCC 15.3 / C++23 toolchain, native XEX packer and Xenos/system APIs are a closer fit for this C++20 project than the legacy XDK route.
- The initial xSDK integration is pinned to commit `9e6de98f6b5d737bce0fecd2864edae94bdd2637`. The repository keeps xSDK external; it is not vendored.
- The Microsoft XDK is proprietary and is not included. It is not required by the preferred path; only consider it later if xSDK has a demonstrated gap. Never commit SDK binaries, headers, libraries, credentials, game images, extracted game data, or generated game translations.
- The technical references are documented in [REFERENCES.md](REFERENCES.md): [Tenkaichi3Decomp](https://github.com/z3xox/Tenkaichi3Decomp) and [BT3XBx](https://github.com/Lucas-Tito/bt3xbx) for PS2 game reconstruction/native Xbox port structure; [Laika64](https://github.com/ErickaguirreD/Laika64) for Xenon MIPS-to-PowerPC dynarec, executable code-cache and Xenos lessons; and [Ibara360](https://github.com/PaulAtreides67/ibara360) for another xSDK-native Xbox title and hardware validation. Treat them as study references, not code/toolchain drop-ins; check file-level provenance and license obligations before adapting code.
- CI run #106 built the smoke XEX successfully after the SIMD oracle test fix. This is a CI build result, not a new physical-console run; run #80 remains the latest hardware evidence.

## Alignment with existing Xbox 360 ports

**Direct precedent — BT3XBx:** this is the closest known project doing the same PS2-to-Xbox-360 class of work. It keeps Xbox code in `port/x360/`, has a staged build/hardware roadmap, uses a deterministic headless replay to check x86/PPC behavior before full console integration, and only promotes optimizations after image/gameplay evidence. Adopt those practices. Do not copy its XDK setup: this project uses the pinned, open xSDK toolchain instead.

**Complementary precedent — Laika64:** this is an N64 emulator for RGH/JTAG, not a PS2 game port. Its MIPS dynarec emits native PowerPC blocks and demonstrates practical Xenon concerns that the current smoke title does not yet test: a writable/executable image section for generated code, data-cache writeback plus instruction-cache invalidation/synchronization, block-cache eviction/invalidation, register caching, and JIT/interpreter lockstep. Its PPC opcode encoder is tested against instructions assembled by GNU `as`. This is valuable for a future hand-emitted/JIT path, but the N64 VR4300 instruction semantics are not the PS2 R5900/Emotion Engine semantics.

**Complementary precedent — Ibara360:** it proves another xSDK-based native XEX can integrate Xenos display, audio, controllers, storage, saves and launcher behavior on real RGH/JTAG hardware. Its game core is different: the arcade's SH-3 program is recompiled and the PS2 Arrange mode is applied as SH-3 patches. It does not provide R5900/EE execution semantics.

The resulting approach is deliberately hybrid in evidence, not in code: BT3XBx guides the PS2-to-Xenon port structure and phase gates; Laika64 guides only Xenon dynarec/code-cache/cache-coherency experiments; Ibara360 and xSDK guide the open SDK, title packaging and console-facing subsystems; PCSX2 and PS2Recomp remain independent references for PS2 instruction behavior and translated-code correctness. Do not import code across these projects without file-level license/provenance review.

## Architecture target

Keep the existing desktop build as the correctness/reference backend. Add an isolated target backend under `port/xbox360/`; do not replace or destabilize the existing PC runtime.

| Subsystem | Xbox 360 direction | Main risk |
|---|---|---|
| Recompiled game code | Generate/compile the retail EE translation for 32-bit big-endian PowerPC | Compiler language support, ABI, floating-point behavior and generated-code size |
| EE runtime | Port the minimum runtime helpers, exceptions, threads, DMA and EE state used by generated code | Existing runtime assumes desktop C++20 facilities |
| Guest memory | Little-endian 8/16/32/64-bit generated RDRAM accessors and a 32 MiB wraparound differential probe now exist; next add full address translation and separate mapped-region handlers | TLB, scratchpad, I/O/GS/VU, DMA and 128-bit semantics |
| GS graphics | New adapter from the project's GS command/state model to xSDK's native Xenos GPU API | PS2 GS blending, depth, texture formats, CLUTs, transfers and effects |
| IOP/services | Port the required SIF/RPC, sound and file services incrementally | Threading, audio decoder and filesystem dependencies |
| Audio | xSDK PCM output initially; evaluate FAudio-xenon for mixing | Timing, resampling, ADPCM and streaming |
| Input | XAM adapter maps Xbox buttons, triggers and sticks into the shared 32-byte PS2 pad report; host tests and a physical Back-to-Select test passed | Second port, vibration and PS2 pressure-sensitive buttons |
| Files/save | xSDK POSIX-style files and `std::filesystem` | PS2 memory-card layout and robust save migration |
| Packaging | xSDK ELF-to-XEX2 packaging and title metadata suitable for Aurora | Title metadata and unsigned XEX support is limited to RGH/JTAG |

### Toolchain feasibility gate

The preferred route is now [xSDK](https://github.com/xenosworks/xsdk), not the legacy Microsoft XDK compiler. Its public documentation reports GCC 15.3, binutils 2.47 and newlib 4.6 for `powerpc-xsdk-elf`, plus a C++23 runtime with exceptions, RTTI, threads, atomics, chrono and filesystem. It also provides CMake helpers to package an ELF as a XEX2 title.

The first target is a small C++20 smoke title in `port/xbox360/`. It exercises the exact language facilities visible in the current source (ranges, span, atomic shared pointers, filesystem and threads), opens Xenos GPU presentation at 720p, maps XAM input to the project's PS2 pad report and includes compile-time checks for a 32-bit big-endian target ABI.

The run #80 smoke XEX, XAM-to-PS2 pad adapter, endian probe and scalar PS2 packed-vector probe were compiled by CI and exercised on the physical console. The supplied log confirms the 32-bit big-endian ABI, C++20 checks, little-endian guest-memory probe, scalar packed-vector probe, display, Back-to-Select mapping and clean shutdown after 439 frames. The scalar PS2 packed-vector reference covers a host-tested subset of arithmetic, comparisons, saturation, shifts, packing and interleave; the target probe exercises only a subset of those operations. This is infrastructure groundwork, not a game build.

Keep the target toolchain scoped to `port/xbox360/`. The top-level compiler/analyzer and retail source-generation tools must run on the host; do not apply the PowerPC CMake toolchain to the whole repository. Build the generated game/runtime sources separately after the host-side generation stage.

If xSDK fails the C++20 smoke build, stop and investigate a specific missing feature before changing the shared runtime. Do not fall back to copying old XDK flags or converting the entire project to C89 without a measured reason.

## Milestones and acceptance criteria

### X0 — Repository and source audit
- Keep the desktop target buildable.
- Inventory the platform-dependent code, third-party libraries, system calls and generated-code requirements.
- Identify all C++20 features and runtime services that block the Xbox toolchain.
- Record the exact toolchain version and reproducible build commands without distributing it.

**Acceptance:** source/dependency matrix, xSDK evaluation and a reproducible C++20 smoke XEX build.

### X1 — Minimal native XEX
- Build `port/xbox360/` with the pinned xSDK toolchain.
- Verify C++20 library features, Xenos presentation, controller input and logging.
- Package the smoke title with a development Title ID and name.
- Launch it from Aurora on an RGH/JTAG console and capture the build log and console result.

**Acceptance:** CI packages the smoke XEX and a console run records title startup, C++20/guest-memory and scalar SIMD checks, display initialization, explicit Back-button exit and clean shutdown. These checks are now recorded in the run #80 console log. Console model/revision and dashboard/kernel metadata remain to be captured in a later compatibility test.

### X2 — Xbox platform layer
- Add platform abstractions for memory, files, timing, threads, input and logging. The initial input mapping and scalar generated-code RDRAM path have host-side tests; the latter is a narrow ABI shim, not yet integrated into the full runtime.
- Keep Xbox-specific code in `port/xbox360/` and guarded target build definitions.
- Add game-data-free unit tests for portable logic.

**Acceptance:** platform layer builds for Xbox and desktop; desktop regression suite stays green.

### X3 — Recompiler/runtime on PowerPC
- Compile a small, deterministic translated EE test program from the same source/vector set for the desktop reference and the PowerPC target.
- Emit a stable state trace at fixed checkpoints: PC, selected 128-bit GPRs, HI/LO, retired-instruction count and guest-memory bytes touched by the test.
- Verify integer, supported MMI, load/store, branch-delay, exception and floating-point behavior before wiring the generated program into the full runtime.
- Measure memory use and frame/tick cost on the console.
- Keep the initial approach ahead-of-time: generate C++ on the host and compile it with the pinned xSDK toolchain. Laika64 shows that a Xenon MIPS-to-PowerPC JIT is possible, but does not by itself justify replacing this simpler path.
- Only if profiling later justifies runtime code emission, add separate acceptance tests for PowerPC emitter words versus GNU `as`, executable-section/permission behavior, and data/instruction-cache synchronization. Laika64's `.jitc` and I-cache procedure are a reference for test design, not code to transplant.

**Acceptance:** the same input trace produces identical integer/register/memory state and documented floating-point tolerances on the desktop and Xenon builds. CI compilation alone is not the acceptance test; a console run must capture the state trace and timing. Any JIT experiment has its own hardware gate and must not become the default path without measured benefit.

**Current progress (2026-11-24):** two distinct probes now cover different layers. The game-data-free 17-instruction EE dispatcher program is shared by the portable host test and the Xbox smoke title; host CI passes with signature `4F63CD2AAFC519CE`, `PC=0x48`, 17 retired instructions and zero failure bits. The XEX containing that test builds in CI but still needs a physical-console run. Separately, `.github/workflows/xbox360-codegen.yml` creates a synthetic MIPS ELF, passes it through `PS2Recompiler::initialize/recompile/generateOutput`, compiles the emitted C++, and runs the scalar function on the desktop `R5900Context`. This generated-code probe checks immediate arithmetic, ADDU, a taken BEQ and its delay slot, and JR RA; it reuses the first four instruction words from the dispatcher program. It is **not** yet a Xenon execution test, a full EE/MMI parity test, or a comparison against a retail game translation. The next target-side gate is to port the generated-code runtime ABI/helpers from the desktop SSE-based `R5900Context` to PPC and link this generated probe into the XEX. See [EE_PARITY_TEST.md](EE_PARITY_TEST.md).

**Additional progress (2026-11-24):** the generated-code ABI now implements explicit little-endian 8/16/32/64-bit accesses against a 32 MiB RDRAM view, including masked wraparound and a small set of direct/KSEG windows. The PS2Recomp fixture now exercises `SB/SH/SW/SD`, `LB/LBU/LHU/LW/LD`, with a word store crossing the RDRAM boundary. CI runs the same emitted function against both the desktop reference runtime and the Xenon ABI shim on the host, and the XEX compiles that function for PowerPC. Scratchpad, MMIO, GS/VU regions, 128-bit memory operations, full TLB translation and physical-console execution remain outstanding; this is not yet safe for the retail game translation.

### X4 — GS renderer
- Implement command submission, state, textures, CLUTs, framebuffer/depth, blending and transfers.
- Start with diagnostic triangles and clear operations, then the opening/menu scenes.
- Validate captured images against the reference renderer.

**Acceptance:** reproducible GS tests pass and the same selected scenes render without corruption on hardware.

### X5 — Boot, frontend and saves
- Read only the user's own extracted retail files.
- Boot to title/menu, navigate with the Xbox controller, save and reload.
- Add movie/audio support only after stable scene rendering.

**Acceptance:** repeatable clean boot and menu/save cycle on console.

### X6 — Playable first stage
- Reach Goro's Lair naturally, render the introduction and allow Liu Kang/Kung Lao control.
- Validate movement, camera, combat, enemies, collisions and progression.
- Profile CPU/GPU/memory and fix frame pacing.

**Acceptance:** documented real-console gameplay, not merely a HUD, menu or scene-drawing trace.

### X7 — Compatibility and release
- Regression-test supported RGH/JTAG configurations.
- Package the XEX and original project metadata; provide an installer/readme.
- Keep all proprietary game data and SDK material out of the release.

**Acceptance:** repeatable install/launch guide and a release whose tested limitations are explicit.

## Engineering rules

1. Keep the desktop runtime as the behavioral reference until the console backend is proven.
2. Follow BT3XBx's phase-gated workflow: deterministic headless parity before full game integration, and hardware evidence before claiming a console milestone.
3. Follow Laika64's testing discipline for any future native code emitter: compare PPC instruction words to an independent assembler and verify generated-code cache coherency on real hardware; do not copy its N64/VR4300 semantics into the EE.
4. Follow Ibara360/xSDK's separation of host-side preparation from the small native XEX and its console-facing services; keep the SDK external and pinned.
5. Keep ahead-of-time recompilation as the default; a runtime JIT is an experiment that requires a measured need and separate approval gates.
6. Do not enable fast-math or floating-point contraction for PS2-derived code until differential tests prove the resulting values are acceptable.
7. Do not introduce guessed guest behavior, fake readiness flags or forced scene skips to hide missing emulation.
8. Every platform-specific workaround needs a focused regression test and hardware evidence where applicable.
9. Do not claim a phase is complete based only on compilation; use the phase's stated acceptance test.
10. Record compiler, commit, game executable hash, configuration, logs and console model/kernel for every hardware result.
11. Do not publish game data, proprietary SDK files, extracted assets, memory dumps or generated translations.
