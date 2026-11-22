# Xbox 360 smoke test report — 2026-11-22

## Source and scope

This report records the user-provided `mksm_xbox360_smoke.log` from the first run of the native xSDK smoke title. It validates only the Xbox 360 executable bootstrap, C++ runtime checks and display initialization. It does not demonstrate that the Mortal Kombat game runtime or any game assets are integrated.

## Observed log

- XEX path reported by the title: `\Device\Mass1\mksm_xbox360_smoke.xex`.
- The title reached `MKSM Xbox 360 smoke: starting`.
- C++20 runtime checks reported `passed`.
- Display output reported `1280x720 scaled to the 1920x1080 output`.
- The line counter reached 720 in 23,182 μs and 32,051 μs in two reported samples.
- The title reported `display opened at 1280x720`.
- Process exit was reported as `status 0`.

The log says no `net_log.txt` was present beside the XEX, so the log was not collected over the network.

## Result

**Launch, C++20 runtime checks and display initialization: passed according to the supplied log.**

The log contains no explicit record of the Back-button event or the reason the loop ended. In the version used for this run, a presentation-begin failure could also leave the process with exit code 0. The smoke title is therefore being updated to log the Back event, frame count and failure exit code explicitly. Re-run that improved build to close the input/clean-shutdown evidence gap.

## Evidence not captured

- Console model/revision and dashboard/kernel version.
- Explicit controller Back-button event.
- Exact number of presented frames.
- An explicit shutdown reason.

No claim is made here about game boot, PS2 instruction execution, GS rendering, audio, saves, or playable gameplay.


## Revised CI build — run #76 (2026-11-22)

GitHub Actions [run #76](https://github.com/dagamavasco170-lang/mksm-recomp-360/actions/runs/45058558103) completed successfully at commit `aecacb822876f3e770d8c4ac337b0c40a0c74c36`.

The run passed:
- Host unit test for Xbox-to-PS2 controller mapping.
- Host unit test for little-endian guest-memory reads/writes and masked wraparound.
- Pinned xSDK PowerPC toolchain build and SDK runtime/tools build.
- CMake configure, compilation of all three Xbox smoke sources, XEX link and artifact upload.
- Compile-time target assertions for 32-bit pointers and big-endian byte order.

Artifact:
- Name: `mksm-xbox360-smoke-xex`
- Artifact ID: `14769905951`
- XEX size: `1,806,336` bytes
- XEX SHA-256: `bf7ba665a13342bbb5c76818888296fae232a214d998e334e6c6022253e593ab`
- ZIP size: `682,740` bytes
- ZIP SHA-256: `f4ab43797a8b8d3b35b8377a2a70f844ef1a19aa42fa402735da8da79ab5b96b`
- GitHub artifact expires on 2026-12-06.

The downloaded XEX was independently identified as a Microsoft Xbox 360 executable. This verifies the build/package, not a new physical-console run. The updated artifact still needs to be launched from Aurora to verify the live XAM controller path, Back-button exit and shutdown log. It remains a smoke title and does not contain the Mortal Kombat game runtime.
