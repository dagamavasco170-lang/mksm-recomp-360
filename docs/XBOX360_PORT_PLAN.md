# Xbox 360 (Xenon/NXDK) Port Plan

This is an engineering plan for a native Xbox 360 target. It is not a claim that the project already builds for Xenon. The current recompiler/runtime is an experimental PS2 MIPS-to-C++ host build; the Xbox 360 target must be introduced incrementally with evidence at each gate.

## Strategy

Do not fork or replace the recompiler. Add a platform target and a thin Xenon host backend, then compile a minimal test before attempting the full game.

Use the existing host build as the reference implementation. Keep PC, Android, and Vita paths working; isolate Xbox 360-specific code behind compile-time target definitions and backend interfaces.

## Target assumptions and constraints

- Hardware: Xbox 360 Xenon PowerPC, big-endian, 32-bit target address space, RGH homebrew execution.
- Toolchain/runtime: NXDK and its supported compiler/linker/runtime; confirm the exact current NXDK build and supported C++/stdlib features before selecting dependencies.
- Output: first a minimal XEX that boots, writes a log, and displays a diagnostic screen; then the runtime smoke target; only later game boot.
- The 360 GPU is Xenos. Existing desktop Raylib backend and FFmpeg desktop packages are not assumed to work on NXDK.

## Gate 0 — Reproducible baseline

Before port work, record:
- current main commit and working-tree state;
- build/test commands and results on the known working host;
- generated-code revision and game-data-free test results.

No retail files, generated game code, assets, or dumps should be uploaded to CI.

## Gate 1 — NXDK hello-XEX (separate target)

Create a separate xbox360/ target/toolchain path. Do not change the default desktop/Vita/Android build.

Acceptance:
- NXDK environment/toolchain detected with a clear failure message when absent;
- builds a minimal XEX;
- boots on RGH hardware or a documented compatible emulator;
- writes a serial/debug log and displays a stable diagnostic screen;
- input can be read from a controller;
- exits/reboots cleanly or has a documented reset behavior.

This proves the toolchain, packaging, launch, video, input, and basic runtime are usable independently of the PS2 runtime.

## Gate 2 — Runtime dependency audit and portability boundary

Audit source and CMake dependency edges. Classify each as portable, replaceable, or unsupported for NXDK.

| Area | Current host implementation | Xenon approach / decision |
|---|---|---|
| Boot/path selection | filesystem and argv path discovery | fixed/configurable game path; audit filesystem support |
| Logging | stdout/stderr and host logging | serial/debug output first; file logging if supported |
| Threading | C++/host threads and scheduler | NXDK-supported threads; single-threaded fallback initially |
| Graphics | Raylib + desktop APIs | minimal Xenos backend; do not bring desktop dependencies blindly |
| Audio | host audio backend + optional FFmpeg | defer; stub audio while proving CPU/runtime |
| MPEG movies | FFmpeg desktop libraries | initially skip/stub movies with traceable diagnostics |
| Controllers | host/gamepad mapping | NXDK controller input to existing PS2 pad abstraction |
| Storage | host filesystem | NXDK filesystem APIs with explicit path translation |
| Memory | host allocations and PS2 guest-memory model | audit alignment, address-width, endian assumptions, allocator limits |
| Debug UI | ImGui/rlImGui | disabled initially; lightweight diagnostics only |

## Gate 3 — Architecture and endian audit

Xenon is PowerPC and big-endian; the current PC build is typically little-endian. Audit before attempting a full game:
- generated MIPS code and data loads/stores;
- byte/halfword/word guest-memory helpers;
- packed structs and serialization;
- floating-point assumptions and NaN behavior;
- pointer-width assumptions, guest address conversion, sign extension;
- SIMD/intrinsics and alignment;
- atomics, barriers, and scheduler synchronization.

Do not use a blanket byte-swap workaround. Guest EE memory semantics must remain explicit and testable.

## Gate 4 — Compile and run a generated-code smoke target

Use a tiny generated translation unit with no game assets to validate:
- Xenon C++ compiler accepts generated code and runtime headers;
- 32-bit guest memory read/write semantics;
- MIPS branch/call/return helpers;
- delay-slot behavior as emitted by the recompiler;
- float and integer test vectors;
- stack and heap behavior;
- one scheduler/timer test.

Acceptance is a deterministic test result plus a runtime log captured from the target.

## Gate 5 — Integrate the PS2 runtime in layers

Build dependencies incrementally, never all at once:
1. memory and CPU/generated-code helpers;
2. scheduler/thread primitives;
3. IOP and RPC services with no media;
4. controller/pad;
5. GIF/VIF/VU and GS state;
6. basic Xenos presentation backend;
7. textures and framebuffers;
8. audio;
9. movie playback;
10. game boot and progression.

Keep the existing desktop renderer as the reference oracle for CPU-side GS/VIF state where possible. A Xenon renderer is a backend, not a reason to rewrite PS2 graphics semantics.

## Gate 6 — Game milestone

Only claim success when each item has evidence:
- retail boot reaches menu;
- controller navigation;
- character/difficulty selection;
- opening movie or documented movie fallback;
- Goro's Lair scene with recognizable geometry and camera;
- player movement and combat;
- save/load and progression;
- audio and cutscenes.

## First implementation task

Start with an NXDK/XEX skeleton and a small CMake/toolchain or NXDK make-based build isolated under xbox360/. Then add a diagnostic test executable that does not include generated game code. The first pull request should only prove boot/log/video/controller and avoid touching the current runtime.

## Risks and stop/go criteria

- NXDK standard library or compiler incompatibilities may require isolating or replacing host services.
- Xenon performance and memory bandwidth make desktop assumptions (thread count, texture cache, FFmpeg, debug UI) unacceptable.
- Big-endian execution is a correctness risk for generated code and guest-memory helpers.
- Existing GS/VIF/VU work is valuable, but the current host graphics backend is not a drop-in Xbox 360 GPU backend.
- If generated code cannot compile or guest-memory semantics cannot be validated on Xenon, stop integration and first build a portable CPU/memory compatibility layer with host tests.

## Fork/reference research

OpenShao forks are references for game-level recompilation changes, SDK/build hygiene, and networking. None of the inspected fork deltas establishes a ready-made Xbox 360/Xenon target. Do not copy PC renderer or x64-only assumptions into NXDK.
