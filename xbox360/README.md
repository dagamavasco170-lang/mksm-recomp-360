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
- [ ] Log reports successful GPU/presentation startup and dimensions
- [ ] Screen remains a stable blue clear while running
- [ ] Back exits to dashboard
- [ ] Log ends with the frame count and GPU close status

This target is not yet wired into the project's default CMake build and does not prove that the PS2 runtime is Xenon-compatible.
