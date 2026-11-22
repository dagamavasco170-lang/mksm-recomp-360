# Xbox 360 smoke test report — 2026-11-22

## Scope

This report covers the native xSDK bootstrap title only. It does not demonstrate that the Mortal Kombat game runtime, retail data, PS2 EE execution, GS rendering, audio, saves or gameplay are integrated.

## First console run

The earlier user-provided log recorded title startup, C++20 runtime checks, 1280×720 display initialization and process exit status 0. It did not record an explicit controller event or shutdown reason.

## Revised console run — run #76 XEX

The second user-provided log, uploaded on 2026-11-22 as mksm_xbox360_smoke(1).log, records:

- XEX launched from the console storage path \Device\Mass1\mksm_xbox360_smoke(1).xex.
- Target ABI check: 32-bit big-endian PowerPC.
- C++20 runtime checks passed.
- Explicit little-endian guest-memory probe passed.
- Display opened at 1280×720; the display path reported scaling to 1920×1080 output.
- XAM-to-PS2 controller adapter initialized for player 1.
- The log explicitly recorded Xbox Back received as PS2 Select.
- 122 frames were presented; shutdown reason was Back; exit code was 0.
- Final process status was 0.

The log also states that no net_log.txt was present beside the XEX, so the log was not collected over the network.

## Result

**The revised smoke title passed its recorded console launch, ABI, C++20, guest-memory endian probe, display, controller mapping and clean Back-button shutdown checks.** This closes the hardware evidence gap left by the first log and verifies the run #76 XEX behavior recorded by the user.

## Remaining evidence gap

The log does not identify the console model/revision or dashboard/kernel version. Those details should be captured in a later compatibility test, but their absence does not change the explicit successful test results above.

## CI build evidence — run #76

GitHub Actions [run #76](https://github.com/dagamavasco170-lang/mksm-recomp-360/actions/runs/45058558103) completed successfully at commit aecacb822876f3e770d8c4ac337b0c40a0c74c36.

The run passed the host controller mapping test, host guest-memory test, pinned xSDK PowerPC toolchain and runtime build, CMake configuration, smoke-source compilation, XEX link and artifact upload. Compile-time assertions for the 32-bit pointer ABI and big-endian target also passed.

Artifact:
- Name: mksm-xbox360-smoke-xex
- Artifact ID: 14769905951
- XEX size: 1,806,336 bytes
- XEX SHA-256: bf7ba665a13342bbb5c76818888296fae232a214d998e334e6c6022253e593ab
- ZIP size: 682,740 bytes
- ZIP SHA-256: f4ab43797a8b8d3b35b8377a2a70f844ef1a19aa42fa402735da8da79ab5b96b
- GitHub artifact expiry: 2026-12-06.

This confirms the smoke bootstrap only. The XEX draws a solid background and contains no game runtime or proprietary game data.
