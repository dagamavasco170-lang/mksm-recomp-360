# Xbox 360 native port plan

## Goal

Produce an **Xbox 360-native homebrew build** of Mortal Kombat: Shaolin Monks for RGH/JTAG consoles. The intended deliverable is an Xbox 360 title package containing `default.xex`, launchable from Aurora.

This follows the broad engineering method demonstrated by the native BT3 Xbox 360 port: recompile the original game's program for the Xbox 360 PowerPC CPU and provide Xbox-specific graphics, audio, input, memory, file and system layers. It is **not** a PS2 emulator packaged for Xbox 360.

This document is a plan and a record of verified constraints. It does not claim that a Shaolin Monks Xbox 360 game executable exists yet.

## Latest smoke result (2026-11-22)

The xSDK smoke XEX was built successfully by GitHub Actions. A user-provided console log records title startup, passing C++20 runtime checks, 1280x720 display initialization (scaled by the display path to 1920x1080 output), and process exit status 0. See [the hardware smoke report](HARDWARE_SMOKE_2026-11-22.md).

This validates the bootstrap title only. The log does not explicitly identify a Back-button event or shutdown reason, and it omits console model/kernel details. The smoke title has been instrumented to record those events and to return a nonzero status for presentation/close failures; that revised build still needs a repeat run. No game code or PS2 GS rendering has run on Xbox as part of this test.

## Source baseline and constraints

See [the initial source/build-graph audit](SOURCE_AUDIT.md) for verified CMake targets, language requirements and current platform dependencies.

- Base project: this repository's PS2 recompilation of the NTSC-U retail release, executable `SLUS_210.87`.
- Required retail executable SHA-256: `b1ed99d81b07969553ed2f9afaad4caed1bbeb637d53a369f4dafca8eedd2ab2`.
- Current public runtime status is still experimental: the opening sequence can reach scene drawing, but first-stage geometry/camera output is badly corrupted and controllable gameplay has not been established. See [STATUS.md](../STATUS.md).
- The desktop runtime currently uses C++20 and desktop-oriented dependencies, including raylib. Those assumptions must not leak into the Xbox target.
- Preferred target SDK: [xenosworks/xsdk](https://github.com/xenosworks/xsdk), evaluated in [SDK_EVALUATION.md](SDK_EVALUATION.md). Its GCC 15.3 / C++23 toolchain, native XEX packer and Xenos/system APIs are a closer fit for this C++20 project than the legacy XDK route.
- The initial xSDK integration is pinned to commit `9e6de98f6b5d737bce0fecd2864edae94bdd2637`. The repository keeps xSDK external; it is not vendored.
- The Microsoft XDK is proprietary and is not included. It is not required by the preferred path; only consider it later if xSDK has a demonstrated gap. Never commit SDK binaries, headers, libraries, credentials, game images, extracted game data, or generated game translations.
- The BT3 port remains a technical reference for the native recompilation method, hardware constraints and acceptance discipline, not a source-code or toolchain drop-in.

## Architecture target

Keep the existing desktop build as the correctness/reference backend. Add an isolated target backend under `port/xbox360/`; do not replace or destabilize the existing PC runtime.

| Subsystem | Xbox 360 direction | Main risk |
|---|---|---|
| Recompiled game code | Generate/compile the retail EE translation for 32-bit big-endian PowerPC | Compiler language support, ABI, floating-point behavior and generated-code size |
| EE runtime | Port the minimum runtime helpers, exceptions, threads, DMA and EE state used by generated code | Existing runtime assumes desktop C++20 facilities |
| Guest memory | Explicit Xbox-safe arena/address translation for EE RAM, scratchpad and mapped regions | Fixed PS2 virtual addresses conflict with Xbox 360 process mappings |
| GS graphics | New adapter from the project's GS command/state model to xSDK's native Xenos GPU API | PS2 GS blending, depth, texture formats, CLUTs, transfers and effects |
| IOP/services | Port the required SIF/RPC, sound and file services incrementally | Threading, audio decoder and filesystem dependencies |
| Audio | xSDK PCM output initially; evaluate FAudio-xenon for mixing | Timing, resampling, ADPCM and streaming |
| Input | xSDK XAM controller input with Start/Back and analog/digital mapping | Button equivalence and pressure-sensitive PS2 inputs |
| Files/save | xSDK POSIX-style files and `std::filesystem` | PS2 memory-card layout and robust save migration |
| Packaging | xSDK ELF-to-XEX2 packaging and title metadata suitable for Aurora | Title metadata and unsigned XEX support is limited to RGH/JTAG |

### Toolchain feasibility gate

The preferred route is now [xSDK](https://github.com/xenosworks/xsdk), not the legacy Microsoft XDK compiler. Its public documentation reports GCC 15.3, binutils 2.47 and newlib 4.6 for `powerpc-xsdk-elf`, plus a C++23 runtime with exceptions, RTTI, threads, atomics, chrono and filesystem. It also provides CMake helpers to package an ELF as a XEX2 title.

The first target is a small C++20 smoke title in `port/xbox360/`. It exercises the exact language facilities visible in the current source (ranges, span, atomic shared pointers, filesystem and threads), opens Xenos GPU presentation at 720p, logs to a file and exits with the controller's Back button.

The smoke title is now cross-compiled into an XEX by CI, and the supplied console log records a successful title start, C++20 checks and display initialization. The original log does not prove the exact controller exit path; repeat the updated diagnostic build and record console model/kernel to complete the X1 evidence. This remains an infrastructure test, not a game build.

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

**Acceptance:** CI packages the smoke XEX and a console run records title startup, C++20 checks, display initialization, explicit Back-button exit and clean shutdown. The first four checks are recorded in the 2026-11-22 log; explicit exit reason and console model/kernel remain to be captured with the revised diagnostic build.

### X2 — Xbox platform layer
- Add platform abstractions for memory, files, timing, threads, input and logging.
- Keep Xbox-specific code in `port/xbox360/` and guarded target build definitions.
- Add game-data-free unit tests for portable logic.

**Acceptance:** platform layer builds for Xbox and desktop; desktop regression suite stays green.

### X3 — Recompiler/runtime on PowerPC
- Compile a small, deterministic translated EE test program.
- Verify integer, load/store, branch, exception and floating-point behavior against the desktop reference.
- Measure memory use and frame/tick cost on the console.

**Acceptance:** the same test vectors and state transitions match the desktop reference within explicitly documented hardware/precision rules.

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
2. Do not introduce guessed guest behavior, fake readiness flags or forced scene skips to hide missing emulation.
3. Every platform-specific workaround needs a focused regression test and hardware evidence where applicable.
4. Do not claim a phase is complete based only on compilation; use the phase's stated acceptance test.
5. Record compiler, commit, game executable hash, configuration, logs and console model/kernel for every hardware result.
6. Do not publish game data, proprietary SDK files, extracted assets, memory dumps or generated translations.
