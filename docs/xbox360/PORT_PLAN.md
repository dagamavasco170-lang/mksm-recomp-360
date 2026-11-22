# Xbox 360 native port plan

## Goal

Produce an **Xbox 360-native homebrew build** of Mortal Kombat: Shaolin Monks for RGH/JTAG consoles. The intended deliverable is an Xbox 360 title package containing `default.xex`, launchable from Aurora.

This follows the broad engineering method demonstrated by the native BT3 Xbox 360 port: recompile the original game's program for the Xbox 360 PowerPC CPU and provide Xbox-specific graphics, audio, input, memory, file and system layers. It is **not** a PS2 emulator packaged for Xbox 360.

This document is a plan and a record of verified constraints. It does not claim that a Shaolin Monks Xbox 360 executable exists yet.

## Source baseline and constraints

See [the initial source/build-graph audit](SOURCE_AUDIT.md) for verified CMake targets, language requirements and current platform dependencies.

- Base project: this repository's PS2 recompilation of the NTSC-U retail release, executable `SLUS_210.87`.
- Required retail executable SHA-256: `b1ed99d81b07969553ed2f9afaad4caed1bbeb637d53a369f4dafca8eedd2ab2`.
- Current public runtime status is still experimental: the opening sequence can reach scene drawing, but first-stage geometry/camera output is badly corrupted and controllable gameplay has not been established. See [STATUS.md](../STATUS.md).
- The desktop runtime currently uses C++20 and desktop-oriented dependencies, including raylib. Those assumptions must not leak into the Xbox target.
- The Xbox 360 XDK/toolchain is proprietary and is not included in this repository. Each developer must obtain it independently under the applicable licence. Never commit SDK binaries, headers, libraries, credentials, game images, extracted game data, or generated game translations.
- The BT3 port is a technical reference, not a source-code drop-in. Its game code, renderer, runtime and build assumptions differ from this project's C++20 recompiler/runtime.

## Architecture target

Keep the existing desktop build as the correctness/reference backend. Add an isolated target backend under `port/xbox360/`; do not replace or destabilize the existing PC runtime.

| Subsystem | Xbox 360 direction | Main risk |
|---|---|---|
| Recompiled game code | Generate/compile the retail EE translation for 32-bit big-endian PowerPC | Compiler language support, ABI, floating-point behavior and generated-code size |
| EE runtime | Port the minimum runtime helpers, exceptions, threads, DMA and EE state used by generated code | Existing runtime assumes desktop C++20 facilities |
| Guest memory | Explicit Xbox-safe arena/address translation for EE RAM, scratchpad and mapped regions | Fixed PS2 virtual addresses conflict with Xbox 360 process mappings |
| GS graphics | New Xenos/Direct3D 9 backend that consumes the project's GS command/state model | PS2 GS blending, depth, texture formats, CLUTs, transfers and effects |
| IOP/services | Port the required SIF/RPC, sound and file services incrementally | Threading, audio decoder and filesystem dependencies |
| Audio | Xbox-native audio output, initially effects before streamed music/movie audio | Timing, resampling, ADPCM and streaming |
| Input | Xbox 360 controller API with Start/Back and analog/digital mapping | Button equivalence and pressure-sensitive PS2 inputs |
| Files/save | Xbox filesystem paths and a documented save container | PS2 memory-card layout and robust save migration |
| Packaging | XDK-linked XEX/title metadata suitable for Aurora | Title metadata and signing/runtime requirements |

### Toolchain feasibility gate

Before porting large portions of the runtime, prove that the selected Xbox toolchain can compile and link a small program using the exact language subset and runtime facilities required by the current source.

The BT3 port's documented XDK flow uses an older PowerPC compiler and converts much of its game code to C89-compatible source. This project is different: its runtime and generated translations are C++20. Do not assume BT3's compiler flags or C89 conversion scripts will compile this code unchanged.

The first investigation must compare:
1. Xbox compiler language level and standard-library availability.
2. Whether generated C++ can be reduced to a supported subset without changing semantics.
3. Whether a supported alternative PowerPC toolchain can produce an XEX that runs on RGH/JTAG hardware.
4. Exception, RTTI, threading, allocation, filesystem and floating-point requirements.
5. Final executable format, imports and XEX packaging.

If this gate fails, stop and revise the architecture before attempting a full port.

## Milestones and acceptance criteria

### X0 — Repository and source audit
- Keep the desktop target buildable.
- Inventory the platform-dependent code, third-party libraries, system calls and generated-code requirements.
- Identify all C++20 features and runtime services that block the Xbox toolchain.
- Record the exact toolchain version and reproducible build commands without distributing it.

**Acceptance:** a source/dependency matrix and a documented, technically supported toolchain choice.

### X1 — Minimal native XEX
- Build a minimal program with the selected toolchain.
- Package it with a unique development Title ID, title name and original project icon.
- Launch it from Aurora on an RGH/JTAG console.
- Capture the build log, console result and exact toolchain version.

**Acceptance:** the test executable appears and launches on real hardware. A successful PC cross-compile alone is not enough.

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
