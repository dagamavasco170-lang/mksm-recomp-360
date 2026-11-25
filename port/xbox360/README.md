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

The generated runtime ABI now handles the two normal-mode EE scratchpad DMA channels (scratchpad→RDRAM and RDRAM→scratchpad): MADR alignment/bit-31 behavior, QWC sizing, 16 KiB SADR wrap, completion registers, D_STAT W1C/mask behavior and D_STADR in stall-control mode. Chain/interleave/MFIFO modes and DMA timing/arbitration remain unsupported. Host regressions cover aliases, both directions, QWC boundaries, disabled DMA and invalid ranges; the smoke XEX also runs a small wraparound DMA probe at startup.

The scalar PS2 packed-vector reference covers signed comparisons, min/max, absolute value, saturation, variable shifts, packing, interleave and PMADDH semantics. The EE dispatcher now executes a deliberately small MMI slice: PMADDH, PMFHI, PMFLO, PMTHI and PMTLO. Host regressions cover HI/LO effects, 128-bit GPR transfers, MMI in a branch delay slot and atomic rejection of unsupported encodings.

A shared, game-data-free EE program is now run by the portable host test and the Xbox smoke title. This dispatcher test executes 17 instructions across immediate arithmetic, SW/LW, ADDU, a taken BEQ with its delay slot, HI/LO transfers and PMADDH. Its host golden signature is `4F63CD2AAFC519CE` (`PC=0x48`, 17 retired instructions); CI passes the host test and builds the XEX containing the same check. The new XEX still needs its own physical-console run, so host/Xenon trace equality remains pending.

A focused workflow sends a synthetic ELF containing three functions through the full PS2Recomp ELF-to-C++ generation pipeline. The first covers scalar arithmetic/branching and RDRAM, the second covers scratchpad and its bit-31 alias, and the third covers the selected MMI HI/LO slice (`PMFHI/PMFLO/PMTHI/PMTLO/PMADDH/PMSUBH`). The desktop reference runner executes the generated functions against the desktop runtime; the Xenon ABI runner executes the same generated source against a host-side Xenon shim. The shim provides low-64-bit GPR accessors, PC/branch-return state, little-endian RDRAM, a separate per-runtime 16 KiB scratchpad, and complete 128-bit HI/LO storage for the tested `PMFHI/PMFLO/PMTHI/PMTLO/PMADDH/PMSUBH` operations. The 12-instruction MMI function checks all eight PMADDH accumulator words, then verifies PMSUBH restores the banks and produces its destination GPR. Both host paths report zero failures, and the exact generated C++ compiles into the xSDK PowerPC XEX. A physical-console run is still required. Other MMI variants (including PHMADH/PHMSBH, PMFHL/PMTHL and several multiply/divide operations), general TLB/COP0, scratchpad DMA/DMAC, MMIO, VU/GS/IOP devices, 128-bit LQ/SQ memory, syscalls, scheduling and platform services remain unsupported or unverified. See [the deterministic EE trace test](../../docs/xbox360/EE_PARITY_TEST.md), [the guest-memory groundwork](../../docs/xbox360/GUEST_MEMORY.md) and [the MMI HI/LO semantics note](../../docs/xbox360/MMI_HILO.md).

This is not a complete MMI implementation, an SSE replacement or a VMX128 backend; the full game runtime remains outside the Xbox target.

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

Copy the XEX to a folder on the RGH/JTAG console using Aurora's FTP service, then launch it from the dashboard's file browser. The screen should show a solid 720p background. The target source reads controller 1 through the XAM adapter, converts it into the same 32-byte PS2 pad report used by the runtime, and exits when Xbox Back arrives as the PS2 Select bit. The run #76 controller path was confirmed on hardware; the newer run #80 XEX also includes scalar SIMD probes and requires its own console retest. The title writes `mksm_xbox360_smoke.log` beside the XEX. The deterministic EE probe should emit `MKSM_EE_TRACE signature=4F63CD2AAFC519CE failures=00000000 pc=00000048 retired=17`. The generated scalar function should also emit `MKSM_RECOMP_XENON_TRACE failures=00000000 pc=CAFE1234 r1=12345678 r2=12345677 r3=12345688 r4=12345688 r5=2468AD10 r6=00000009 r7=0000000C`. Compare both traces with CI and retain the log plus console/kernel details. The revised XEX still needs its first physical-console execution.

Unsigned xSDK titles require RGH/JTAG. They do not run on an unmodified retail console.

## What comes next

The run #80 smoke XEX passed its recorded console checks, but it predates the current generated-code ABI and scratchpad changes. The current generated ABI has scalar RDRAM and separate 16 KiB scratchpad load/store helpers with host regression coverage; its new XEX has not yet been exercised on a physical console. The first dispatcher-connected MMI slice remains host-tested only. The next memory gate is to execute the revised XEX on a physical RGH/JTAG console and confirm the generated-memory trace plus `MKSM_SPR_DMA_TRACE`. This implementation is a synchronous normal-mode SPR DMA model only; then continue with separately tested DMAC channel families, device MMIO and runtime services in isolated slices. Keep host-side recompilation/generation separate from the PowerPC target build. See [the port roadmap](../../docs/xbox360/PORT_PLAN.md), [the source audit](../../docs/xbox360/SOURCE_AUDIT.md), [the guest-memory groundwork](../../docs/xbox360/GUEST_MEMORY.md), [the SIMD compatibility audit](../../docs/xbox360/SIMD_COMPATIBILITY.md) and [the SDK evaluation](../../docs/xbox360/SDK_EVALUATION.md).
