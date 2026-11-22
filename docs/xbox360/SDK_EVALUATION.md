# Xbox 360 SDK evaluation

## Decision

Use **[xenosworks/xsdk](https://github.com/xenosworks/xsdk)** as the preferred first target SDK, subject to a smoke build and a real-console launch. Keep the original XDK/BT3 build route as a reference only; do not make the proprietary Microsoft SDK a project dependency unless a concrete xSDK gap requires it.

This is a better fit for MKSM Recomp than the legacy XDK compiler path because the current project requires C++20 and xSDK documents a C++23 runtime.

## Why xSDK fits

The evaluated upstream is pinned for this first integration study to commit:

`9e6de98f6b5d737bce0fecd2864edae94bdd2637`

Its public documentation reports:

- A GCC 15.3 / binutils 2.47 / newlib 4.6 toolchain targeting `powerpc-xsdk-elf`, with the Xbox calling convention patched into the toolchain.
- A C++23 runtime with exceptions, RTTI, threads, atomics, chrono, filesystem and format.
- CMake integration through `cmake/xsdk.cmake` and `xsdk_add_xex()`; the SDK packages an ELF into a XEX2 title with an ID, name and version.
- A native Xenos GPU API for draws, render targets, depth/stencil, blending, textures, shader loading and presentation.
- Controller input through XAM, threads, files, saves and 720p presentation paced to vblank.
- Audio output of six-channel PCM at 48 kHz. FAudio-xenon is a related project for XAudio2-style mixing.

Sources: [xsdk README](https://github.com/xenosworks/xsdk), [getting started](https://github.com/xenosworks/xsdk/blob/main/docs/GETTING_STARTED.md), [C++/system parity](https://github.com/xenosworks/xsdk/blob/main/docs/PARITY.md#c-and-c-runtime), [GPU parity](https://github.com/xenosworks/xsdk/blob/main/docs/PARITY.md#gpu), [audio parity](https://github.com/xenosworks/xsdk/blob/main/docs/PARITY.md#audio).

## Important limitations

- xSDK's GPU API is a native Xenos command interface, **not** a PS2 GS implementation and not a drop-in replacement for raylib.
- The current desktop audio backend is coupled to the host API. It must be adapted to xSDK's audio queue.
- Current desktop media support links FFmpeg. Movie decoding/streaming must be assessed separately; do not assume the desktop FFmpeg configuration cross-compiles.
- xSDK's documented PCM output is not the same thing as hardware XMA decoding. MKSM's VAG/ADPCM decode and mixing still need correct integration.
- Unsigned XEX titles run on RGH/JTAG, not an unmodified retail console.
- The xSDK documentation records features that are still incomplete. Re-check its parity table and TODO before depending on any optional API.

## How it maps to MKSM

| MKSM requirement | xSDK facility | Port work still required |
|---|---|---|
| C++20 runtime | C++23 libstdc++ | Build and test the current source subset; audit unsupported compiler builtins/ABI assumptions |
| XEX packaging | `xsdk_add_xex`, `elf2xex.py` | Title metadata and release packaging |
| GPU presentation | `xsdk_present_*` | GS framebuffer and presentation adapter |
| PS2 GS renderer | Xenos GPU, textures and shaders | Translate GS state, texture formats, transfers, blending and raster rules |
| Gamepad | `XamInputGetState` | Map PS2 pad protocol and pressure/analog values |
| Audio output | `xsdk_audio_*` | Replace raylib callback path with a bounded PCM feeder |
| Files/save | POSIX files and `std::filesystem` | Map retail game paths and PS2 memory-card/save semantics |
| Threads/synchronization | pthreads and `std::thread` | Audit scheduler priorities and timing behavior |
| Movies | No complete MKSM-specific path established | Decide on a supported decoder after boot/gameplay is stable |

## Build split

Do **not** apply the Xbox CMake toolchain to the repository's top-level build. The recompiler/analyzer and source-generation tools are host programs; the generated game/runtime target is the PowerPC target.

Use two build stages:

1. Host build on Windows/Linux to regenerate and validate the retail translations and game-data-free reference tests.
2. Target build in `port/xbox360/` with `CMAKE_TOOLCHAIN_FILE=<xsdk>/cmake/xsdk.cmake`. Build only the target runtime and its generated sources there.

The first code in `port/xbox360/` is a C++20 smoke title. It intentionally does not link the game runtime yet: it tests the SDK toolchain's C++ facilities, GPU presentation, controller exit and XEX packaging in isolation.

## License and source hygiene

xSDK is MIT-licensed, with additional third-party notices for incorporated BSD-licensed files. This project must retain those notices if it later vendors or redistributes any xSDK code. At present, xSDK remains an external build prerequisite and no SDK source is copied into this repository.

Do not add the proprietary XDK, game ISOs, extracted retail files, game assets, private memory dumps or generated retail translations to Git.
