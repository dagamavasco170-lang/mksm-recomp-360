# Xbox 360 source audit — initial inventory

This audit is based on the checked-in build files on the `main` branch. It is a source/build-graph inventory, not a completed compiler or hardware test.

## Verified build facts

- The root CMake project supports the desktop runtime, an Android-only runtime configuration and an existing Vita toolchain path. There is no Xbox 360 platform option or Xbox-specific target.
- `ps2xRuntime/CMakeLists.txt` requires **C++20**.
- `ps2xIOP/CMakeLists.txt` also requires **C++20**.
- The desktop host backend is an interface target named `ps2_host_backend`; it links raylib on the ordinary desktop path. The renderer therefore cannot be assumed to work on Xbox merely because the game/runtime sources compile.
- The runtime target links `ps2_host_backend`, FFmpeg and `ps2_iop`.
- The desktop runtime's build graph includes these central subsystems:
  - EE/runtime: `src/lib/ps2_runtime.cpp`, `src/lib/ps2_memory.cpp`
  - GS: `src/lib/gs/ps2_gs_memory.cpp`, `src/lib/gs/gs_frontend.cpp`, `src/lib/gs/gs_cpu_backend.cpp`, `src/lib/gs/ps2_gif_arbiter.cpp`
  - VIF/VU: `src/lib/ps2_vif1_interpreter.cpp`, `src/lib/vu/ps2_vu1_core.cpp`, `src/lib/vu/ps2_vu1_upper.cpp`, `src/lib/vu/ps2_vu1_lower.cpp`
  - I/O and input: `src/lib/ps2_iop_host.cpp`, `src/lib/ps2_pad.cpp`
  - Audio: `src/lib/ps2_audio.cpp`, `src/lib/ps2_audio_vag.cpp`
  - Game-specific overrides: `src/lib/game_overrides.cpp`
- The desktop runner is `ps2EntryRunner`; it includes the runtime and platform-specific runner sources. Android and Vita already have dedicated source additions, providing examples of existing platform isolation.
- The IOP library contains PS2 service/module implementations and is a separate CMake target, but it also explicitly requires C++20.

## Main port blockers

| Area | Evidence in current build | Consequence |
|---|---|---|
| Compiler/language | Runtime and IOP require C++20 | Validate the Xbox compiler and standard library before porting sources |
| Renderer | Desktop host backend links raylib; GS state is implemented in runtime files | Add a new backend rather than trying to use the desktop host backend on the console |
| Movie/audio dependencies | Runtime links FFmpeg and has VAG/IOP audio paths | Replace host media output/decoding incrementally with console-compatible implementations |
| Platform APIs | Existing Android/Vita runner code is conditional, but no Xbox path exists | Keep new Xbox adapters isolated and avoid desktop APIs in shared code |
| CPU/memory semantics | Project reconstructs PS2 EE/VU state and translated retail functions | Test PowerPC compilation, alignment, endianness, exact memory semantics and floating-point behavior |
| Hardware acceptance | Existing MKSM public status does not establish playable gameplay | Native console boot is only an early milestone, not project completion |

## Recommended order

1. Establish a licensed Xbox toolchain and build a minimal XEX independently of the game.
2. Create a small C++20 compatibility spike using the exact features used by the runtime.
3. If the toolchain is viable, add an Xbox build option and a new `ps2_xbox360_backend` interface; keep `ps2_host_backend` intact.
4. Bring up logging, timers, files, memory and input before graphics.
5. Port the GS backend and validate deterministic graphics tests before attempting the full opening scene.
6. Add IOP/audio/movie components only as the game's verified boot path requires them.

## Reproducibility note

The next source audit should record the exact repository commit, compiler version, CMake options and dependency versions used. No Xbox toolchain build or hardware run has been performed as part of this documentation change.


## Hardware smoke follow-up — 2026-11-22

The source/build-graph findings above remain valid. A later xSDK smoke XEX build passed CI, and a user-provided console log records title startup, successful C++20 runtime checks, 1280x720 display initialization and process exit status 0. The run is documented in [HARDWARE_SMOKE_2026-11-22.md](HARDWARE_SMOKE_2026-11-22.md).

This evidence validates only the console bootstrap path. It does not validate the MKSM runtime, translated PowerPC game code, guest memory, PS2 GS-to-Xenos rendering, IOP, audio, saves or gameplay. The original log also lacks explicit Back-button/shutdown-reason and console model/kernel details.


## Xbox-specific follow-up — controller input and SIMD

- Added an isolated XAM adapter at `port/xbox360/ps2_pad_xbox360.cpp`. It polls player 1/player 2 and produces the existing 32-byte PS2 pad protocol report; the shared mapping logic is in `ps2_pad_mapping.cpp`.
- The adapter is wired into the Xbox smoke target. A host-side unit test covers neutral/disconnected state, face buttons, D-pad, shoulders, triggers and stick axis conversion. The test passed again in workflow run #76, and the same run successfully compiled and packaged the PowerPC XEX with the adapter.
- The adapter deliberately does not claim PS2 pressure-sensitive button support or vibration.
- The current standalone endian helper at `port/xbox360/ps2_guest_memory.h` has host-side tests for scalar little-endian reads/writes, RDRAM-style masked wrap, and raw 128-bit byte order. These tests passed in run #76; the target-side probe was also compiled into that XEX. It is not yet connected to `PS2Memory` or generated-code memory accessors.
- A more serious runtime build blocker has been documented in [SIMD_COMPATIBILITY.md](SIMD_COMPATIBILITY.md): the shared runtime's public state and source still depend on x86 SSE types/intrinsics, and the PowerPC target has no compatibility implementation yet.

The smoke target remains isolated and does not link the full PS2 runtime. `smoke.cpp` has compile-time assertions for a 32-bit pointer and big-endian target; those assertions compiled successfully in run #76. The physical console still needs to launch the revised XEX.
