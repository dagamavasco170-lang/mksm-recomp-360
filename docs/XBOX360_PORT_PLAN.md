# Xbox 360 (Xenon) Port Plan

This is an engineering plan for a native Xbox 360 RGH target. It is not a claim that the project already builds for Xenon. The current recompiler/runtime is an experimental PS2 MIPS-to-C++ host build; the Xbox 360 target must be introduced incrementally with evidence at each gate.

## Strategy

Do not fork or replace the recompiler. Add a platform target and a thin Xenon host backend, then compile a minimal test before attempting the full game.

Use the existing host build as the reference implementation. Keep PC, Android, and Vita paths working; isolate Xbox 360-specific code behind target definitions and backend interfaces.

## Toolchain decision (corrected)

- **Primary investigation target: xsdk** — https://github.com/xenosworks/xsdk. This is a clean-room SDK for Xbox 360 retail-kernel titles, producing XEX files for RGH/JTAG and includes C++ runtime facilities, controller, filesystem, audio, logging, and Xenos GPU work. Its toolchain and maturity still must be validated against our needs.
- **Do not use NXDK for this port.** NXDK is for the original Xbox, not Xbox 360; its XBE target is wrong for Xenon.
- **OXDK is an alternative research reference** — https://github.com/Horrible-Dev/OXDK. Its Xbox 360 path can build XEX2 but depends on a supplied Microsoft XDK, and its documented C++ limitations include no exceptions and limited thread support. This makes it a less suitable default for our clean-room RGH project.
- Keep SDK integration out of the root build until a standalone sample has been built and run successfully.

## Target assumptions and constraints

- Hardware: Xbox 360 Xenon PowerPC, big-endian, 32-bit address space, RGH homebrew execution.
- The compiler/ABI, exception support, thread support, libc++/libc, filesystem, and linker behavior must be proven with small probes before using them in the runtime.
- Output: first a minimal XEX that boots, writes a log, and displays a diagnostic screen; then the runtime smoke target; only later game boot.
- The 360 GPU is Xenos. Existing desktop Raylib backend and FFmpeg desktop packages are not assumed to work on the target.

## Gate 0 — Reproducible baseline

Before port work, record:
- current main commit and working-tree state;
- build/test commands and results on the known working host;
- generated-code revision and game-data-free test results.

No retail files, generated game code, assets, or dumps should be uploaded to CI.

## Gate 1 — xsdk hello-XEX (separate target)

Create an isolated xbox360/ build path based on an xsdk sample/template. Do not change the default desktop/Vita/Android build.

Acceptance:
- toolchain/setup detected with clear failure messages when absent;
- builds a minimal XEX;
- boots on RGH hardware (Xenia is a smoke-test aid, not final proof);
- writes a debug/network log and displays a stable diagnostic screen;
- controller input can be read;
- reset/exit behavior is documented.

This proves the toolchain, XEX packaging, boot, display, input, and basic runtime are usable independently of the PS2 runtime.

## Gate 2 — Runtime dependency audit and portability boundary

Audit source and CMake dependency edges. Classify each as portable, replaceable, or unsupported for Xenon.

| Area | Current host implementation | Xenon approach / decision |
|---|---|---|
| Boot/path selection | filesystem and argv path discovery | fixed/configurable game path; audit SDK filesystem |
| Logging | stdout/stderr and host logging | xsdk logging/debug output first; file logging if supported |
| Threading | C++/host threads and scheduler | verify xsdk thread API and C++ support; single-thread fallback initially |
| Graphics | Raylib + desktop APIs | minimal Xenos backend; do not bring desktop dependencies blindly |
| Audio | host audio backend + optional FFmpeg | defer; stub audio while proving CPU/runtime |
| MPEG movies | FFmpeg desktop libraries | initially skip/stub movies with traceable diagnostics |
| Controllers | host/gamepad mapping | xsdk controller input to existing PS2 pad abstraction |
| Storage | host filesystem | xsdk filesystem APIs with explicit path translation |
| Memory | host allocations and PS2 guest-memory model | audit alignment, address-width, endian assumptions, allocator limits |
| Debug UI | ImGui/rlImGui | disabled initially; lightweight diagnostics only |

## Gate 2.1 — Concrete portability findings

The first source inspection confirms that the runtime cannot be brought over by toggling a platform flag:

- `ps2xRuntime/include/ps2_runtime.h` is a large shared umbrella header that directly includes EE memory, GS frontend, VU1, audio, pad, IOP types, and x86 SIMD headers (`immintrin.h` / `smmintrin.h`, or the existing SSE2NEON shim). The Xenon PowerPC toolchain does not provide the same x86 intrinsic ABI; this is an early compile blocker and should be isolated before compiling the runtime.
- `ps2xRuntime/include/ps2_host_backend.h` directly includes Raylib. Desktop presentation/input/audio are therefore host dependencies, not an Xbox-ready abstraction.
- Runtime CMake currently fetches Raylib, and on desktop also builds the ImGui/rlImGui debug UI. These dependencies must be excluded from the Xenon target, not merely linked differently.
- `ps2xRuntime/src/main.cpp` contains desktop/Android startup, filesystem/argv discovery, logging, and exception termination setup. Xenon needs a dedicated entry point and explicit game/config paths.
- The game-specific generated/runtime folder is separate (`games/mk-shaolin-monks/runtime`), which is useful for building a target-specific launcher without disturbing the PC executable.

This is a source-level audit only; it does not establish that these dependencies compile or run on Xenon.


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

Start from the xsdk sample/template and establish a standalone XEX skeleton under xbox360/. Then add a diagnostic test executable that does not include generated game code. The first change should only prove boot/log/video/controller and avoid touching the current runtime.

## Risks and stop/go criteria

- C++ library/compiler incompatibilities may require isolating or replacing host services.
- Xenon performance and memory bandwidth make desktop assumptions (thread count, texture cache, FFmpeg, debug UI) unacceptable.
- Big-endian execution is a correctness risk for generated code and guest-memory helpers.
- Existing GS/VIF/VU work is valuable, but the current host graphics backend is not a drop-in Xbox 360 GPU backend.
- If generated code cannot compile or guest-memory semantics cannot be validated on Xenon, stop integration and first build a portable CPU/memory compatibility layer with host tests.

## Fork/reference research

OpenShao forks are references for game-level recompilation changes, SDK/build hygiene, and networking. None of the inspected fork deltas establishes a ready-made Xbox 360/Xenon target. Do not copy PC renderer or x64-only assumptions into xsdk.
