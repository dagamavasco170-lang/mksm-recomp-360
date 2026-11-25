# Xbox 360 smoke target

This is an isolated XEX test for the Xbox 360 RGH target using xenosworks/xsdk. It intentionally does not include the PS2 recompiled game or any game data. It verifies SDK linkage, GPU/display startup, presentation loop and controller input before porting runtime code.

## Prerequisites

Follow the upstream xsdk setup/build guide: https://github.com/xenosworks/xsdk/blob/main/docs/GETTING_STARTED.md

The upstream documented environment is Linux or Debian under WSL2, with its GCC 15.3 Xenon/PPC toolchain and xsdk built first. NXDK is for the original Xbox, not Xbox 360.

## Build

From the repository root, after completing the upstream toolchain setup:

```sh
cmake -S xbox360/smoke -B xbox360/build -G Ninja \
  -DCMAKE_TOOLCHAIN_FILE="$HOME/xsdk/cmake/xsdk.cmake" \
  -DXSDK_TOOLCHAIN="$HOME/xsdk-gcc" \
  -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build xbox360/build
```

The resulting XEX is under the build directory. Deploy it to an RGH/JTAG console using the upstream xsdk workflow. Xenia is only a preliminary smoke check; a console run is the acceptance test.

## Acceptance

- [ ] XEX builds using the documented xsdk toolchain
- [ ] XEX launches on RGH/JTAG
- [ ] Log reports the display dimensions and non-zero display check
- [ ] Screen remains a stable blue clear (`0xFF203060`) while running
- [ ] The log's pixel check reports `PASS` for the clear colour
- [ ] Back exits cleanly
- [ ] Final log reports 0 failed checks and `mksm-xbox360-smoke: PASS`

The smoke test waits for the GPU fence, reads the center pixel back from the resolved frame and compares it with the expected clear colour. This verifies more than a launch or a visible screen: it exercises the GPU resolve, presentation buffer and texture download path.

## Run and report back

Copy `xbox360/build/mksm_xbox360_smoke.xex` to the console (FTP is fine), launch it from Aurora or another dashboard, wait a few seconds, then press **Back**. Retrieve `mksm_xbox360_smoke.log` beside the XEX and share it here. A successful run should include lines similar to:

```
PASS: display dimensions are non-zero
PASS: first resolved frame contains the clear colour
PASS: GPU closed cleanly
PASS: at least one frame presented
PASS: frame loop did not fail
PASS: clear-colour pixel verification ran
smoke: frames=... checks=6 passed, 0 failed
mksm-xbox360-smoke: PASS
```

The exact frame count varies. The XEX has not yet been built or tested on hardware by this project. This target is not wired into the project's default CMake build and does not prove that the PS2 runtime is Xenon-compatible.

## PS2 guest-memory endian smoke (host-portability check)

A second isolated test validates explicit little-endian helpers for PS2 guest memory. It is separate from the XEX/GPU smoke and does not require xsdk:

```sh
cmake -S xbox360/core-smoke -B build/guest-endian
cmake --build build/guest-endian
ctest --test-dir build/guest-endian --output-on-failure
```


See [core-smoke README](core-smoke/README.md) and [endian audit](../docs/XBOX360_ENDIAN_AUDIT.md). The scalar little-endian helpers have now been integrated into PS2Memory's shared scalar load/store routines. A focused host compiler check passed for 16/32/64-bit values; the full runtime regression suite and Xenon cross-build remain pending.
