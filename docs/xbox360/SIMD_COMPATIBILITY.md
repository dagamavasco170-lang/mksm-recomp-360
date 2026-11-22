# Xbox 360 SIMD compatibility audit

## Finding

The desktop runtime is not yet source-compatible with the PowerPC Xbox 360 compiler. The smoke executable does not include or link the PS2 runtime, so its successful build must not be interpreted as proof that the complete runtime can compile.

The existing runtime has a substantial architecture-specific SIMD dependency. A targeted source inventory of the core SIMD header and runtime/memory files found 71 distinct `_mm_*` operation names in `ps2_runtime_macros.h` alone (167 references in that header), plus further SSE use in `ps2_runtime.h`, `ps2_runtime.cpp` and `ps2_memory.cpp`. This is not yet an exhaustive scan of every runtime file:

- `ps2xRuntime/include/ps2_runtime.h` and `ps2xRuntime/include/runtime/ps2_memory.h` select `<immintrin.h>` and `<smmintrin.h>` unless compiling with MSVC or the ARM-only `USE_SSE2NEON` path.
- `ps2xRuntime/include/ps2_runtime.h` stores EE/VU state using `__m128i` and `__m128`.
- `ps2xRuntime/src/lib/ps2_runtime.cpp` calls SSE intrinsics such as `_mm_castsi128_ps`, `_mm_set1_epi32`, unaligned vector loads/stores and lane extraction; `ps2xRuntime/src/lib/ps2_memory.cpp` also uses 128-bit unaligned memory operations.
- `ps2_runtime_macros.h` is not just a container wrapper: it translates PS2 MMI packed byte/halfword/word operations (interleave, pack, shuffle, add/subtract, min/max, comparisons, bitwise operations and variable shifts) to SSE. It also defines four-lane floating-point VU helpers.
- The root CMake file enables `USE_SSE2NEON` for ARM targets; it has no corresponding PowerPC vector compatibility path.

The selected xSDK target is `powerpc-xsdk-elf`, not x86. Therefore the current x86 intrinsic types and headers cannot be assumed to exist in the target compiler. This is a concrete integration blocker that must be resolved before adding `ps2xRuntime` to an Xbox executable.

## Recommended implementation

1. Finish an exhaustive inventory across runtime, IOP, GS, VU, generated-code templates and emitted code. Record each operation's signedness, lane width/order, saturation, shift rules and alignment assumptions.
2. Split the compatibility boundary into two semantic groups: PS2 MMI packed integer operations, and PS2 VU four-lane floating-point operations. Keep CPU state and memory load/store representation explicit.
3. Introduce a small `ps2_simd` API. Keep the desktop implementation backed by the existing SSE operations so the PC reference path does not change.
4. Add a scalar reference backend first. Use it to write game-data-free tests for byte/halfword/word interleave and pack order, signed/unsigned arithmetic, saturation, bitwise masks, variable shifts, lane extraction, float bitcasts, and unaligned guest-memory load/store behavior.
5. Implement the PowerPC backend against instructions and ABI supported by the pinned Xenon toolchain. Do not assume ordinary AltiVec is identical to Xenon's VMX128 or that host compiler vector types have the required lane semantics.
6. Run the same deterministic vectors through the desktop scalar/SSE path and the Xenon implementation before migrating large runtime structures or generated game code.

## Initial inventory (not exhaustive)

| Source | Observed SIMD use |
|---|---|
| `ps2_runtime_macros.h` | 167 SSE-operation references across 71 distinct intrinsic names; PS2 MMI integer macros and VU float macros |
| `ps2_runtime.h` | `__m128i` / `__m128` CPU/VU state, lane extraction and 64-bit register construction |
| `ps2_runtime.cpp` | vector state save/restore, float bitcasts, lane extraction and vector loads/stores |
| `ps2_memory.cpp` | 128-bit read/write paths over RDRAM, scratchpad and VU memory |

The targeted inventory inspected the files above and `ps2_vif1_interpreter.cpp`. It has not yet scanned every runtime translation unit or generated game unit, so the counts are lower bounds for the porting effort, not a complete total.

## Scope and status

This is an audit and a proposed migration boundary, not an implemented SIMD port. No SIMD emulation, VMX128 implementation or runtime-wide compiler compatibility is claimed yet. The desktop runtime remains unchanged.

## Related verified work

The first isolated Xbox controller adapter is separate from raylib and uses the common `ps2::pad` encoder. Its mapping code is host-testable and is compiled into the Xbox smoke title. See [the port plan](PORT_PLAN.md) and [the smoke build instructions](../../port/xbox360/README.md).
