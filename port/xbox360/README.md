# Xbox 360 backend

This directory contains the first native Xbox 360 target for MKSM Recomp.

## Current state

The first target is a **C++20 smoke title**, not the game yet. It tests:

- xSDK's C++ standard-library facilities used by the project: ranges, span, atomic shared pointers, filesystem and threads.
- Xenos GPU initialization and 720p presentation.
- XAM controller polling translated into the project's existing 32-byte PS2 pad report, including buttons, triggers and stick axes.
- Per-title logging.
- A host-run unit test for disconnected/neutral reports, face-button mapping, shoulders, D-pad, triggers and signed stick extremes.
- Explicit little-endian guest-memory helpers for 8/16/32/64-bit values and raw 128-bit byte blocks, with wrapped-address tests.

It draws a solid background only. It does not load the retail game, render the PS2 GS, or contain any game data.

The revised run #76 XEX has now been tested on the physical console. The supplied log records ABI, C++20 and little-endian guest-memory checks, 1280×720 presentation, the Xbox Back → PS2 Select event, 122 presented frames and clean shutdown (exit 0). See [the hardware report](../../docs/xbox360/HARDWARE_SMOKE_2026-11-22.md).

The first scalar PS2 packed-vector reference now covers wrapping 8/16/32-bit lane arithmetic, logic, equality masks and low/high interleave. Host tests exercise the same header, and the smoke XEX compiles and runs a small target-side probe. This is not yet an SSE replacement or a VMX128 backend; the full runtime remains outside the Xbox target.

## Preferred SDK

Use [xenosworks/xsdk](https://github.com/xenosworks/xsdk), pinned for this integration to `9e6de98f6b5d737bce0fecd2864edae94bdd2637`. The SDK is external and MIT-licensed; its own third-party notices remain applicable if it is later vendored or redistributed. Do not add proprietary Microsoft XDK files.

## Build on Windows with WSL 2 or Linux

The upstream xSDK currently documents Debian 13 / Linux or WSL 2 as the tested build environment. Allow about 10 GB free. Install the prerequisites inside Debian/WSL:

```sh
sudo apt update
sudo apt install build-essential bison flex texinfo libgmp-dev libmpfr-dev libmpc-dev \
  curl xz-utils patch python3 cmake ninja-build git
```

Clone the pinned SDK revision and build its PowerPC toolchain:

```sh
git clone https://github.com/xenosworks/xsdk.git "$HOME/xsdk"
git -C "$HOME/xsdk" checkout 9e6de98f6b5d737bce0fecd2864edae94bdd2637

cd "$HOME/xsdk"
PREFIX="$HOME/xsdk-gcc" WORK="$HOME/xsdk-gcc-work" JOBS="$(nproc)" \
  tools/toolchain/build.sh
make -j"$(nproc)"
```

Build the smoke title from the root of this MKSM repository:

```sh
cmake -S port/xbox360 -B build/xbox360 -G Ninja \
  -DCMAKE_TOOLCHAIN_FILE="$HOME/xsdk/cmake/xsdk.cmake" \
  -DXSDK_TOOLCHAIN="$HOME/xsdk-gcc" \
  -DCMAKE_BUILD_TYPE=RelWithDebInfo

cmake --build build/xbox360 --parallel
```

Expected output: `build/xbox360/mksm_xbox360_smoke.xex`.

A GitHub Actions workflow builds this target on updates to the `xbox360/**` branch family or on a pull request. It uploads the XEX as the `mksm-xbox360-smoke-xex` artifact when the build succeeds. The artifact is a smoke-test title, not the game.

## Hardware test

Copy the XEX to a folder on the RGH/JTAG console using Aurora's FTP service, then launch it from the dashboard's file browser. The screen should show a solid 720p background. The target source reads controller 1 through the XAM adapter, converts it into the same 32-byte PS2 pad report used by the runtime, and exits when Xbox Back arrives as the PS2 Select bit. CI run #76 compiled this path into the XEX; the actual XAM button behavior still requires verification on hardware. The title writes `mksm_xbox360_smoke.log` beside the XEX; retain the log and the exact console/kernel details with the test report.

Unsigned xSDK titles require RGH/JTAG. They do not run on an unmodified retail console.

## What comes next

The revised XAM/ABI/endian smoke build has passed its recorded console run. The next software gate is to expand the scalar SIMD reference and build a full guest-memory/address-translation layer before integrating runtime services, audio and the GS-to-Xenos renderer. Keep host-side recompilation/generation separate from the PowerPC target build. See [the port roadmap](../../docs/xbox360/PORT_PLAN.md), [the source audit](../../docs/xbox360/SOURCE_AUDIT.md), [the guest-memory groundwork](../../docs/xbox360/GUEST_MEMORY.md), [the SIMD compatibility audit](../../docs/xbox360/SIMD_COMPATIBILITY.md) and [the SDK evaluation](../../docs/xbox360/SDK_EVALUATION.md).
