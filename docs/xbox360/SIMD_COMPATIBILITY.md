# Xbox 360 SIMD compatibility audit

## Finding

The desktop runtime is not yet source-compatible with the PowerPC Xbox 360 compiler. The smoke executable does not include or link the PS2 runtime, so its successful build must not be interpreted as proof that the complete runtime can compile.

The existing runtime has an architecture-specific SIMD dependency:

- `ps2xRuntime/include/ps2_runtime.h` and `ps2xRuntime/include/runtime/ps2_memory.h` select `<immintrin.h>` and `<smmintrin.h>` unless compiling with MSVC or the ARM-only `USE_SSE2NEON` path.
- `ps2xRuntime/include/ps2_runtime.h` stores EE/VU state using `__m128i` and `__m128`.
- `ps2xRuntime/src/lib/ps2_runtime.cpp` calls SSE intrinsics such as `_mm_castsi128_ps` and `_mm_set1_epi32`.
- The root CMake file enables `USE_SSE2NEON` for ARM targets; it has no corresponding PowerPC vector compatibility path.

The selected xSDK target is `powerpc-xsdk-elf`, not x86. Therefore the current x86 intrinsic types and headers cannot be assumed to exist in the target compiler. This is a concrete integration blocker that must be resolved before adding `ps2xRuntime` to an Xbox executable.

## Recommended implementation

1. Inventory every SSE/SSE4 intrinsic and vector type used by the runtime, generated code, memory, VU and GS layers. Record the operation semantics and alignment assumptions.
2. Introduce a small compatibility layer for the subset actually used by MKSM. Keep the desktop implementation backed by the existing SSE intrinsics so the PC reference path does not change.
3. Add a scalar reference implementation for the compatibility layer and game-data-free tests for lane ordering, signed/unsigned arithmetic, bitcasts, saturation, shifts, comparisons, masks and memory alignment.
4. Implement the PowerPC backend against the instructions and ABI supported by the pinned Xenon toolchain. Do not assume ordinary AltiVec is identical to Xenon's VMX128 or that compiler vector types have the required lane semantics.
5. Compile and run deterministic vector test cases on the target before migrating the large runtime structures or generated game code.

## Scope and status

This is an audit and a proposed migration boundary, not an implemented SIMD port. No SIMD emulation, VMX128 implementation or runtime-wide compiler compatibility is claimed yet. The desktop runtime remains unchanged.

## Related verified work

The first isolated Xbox controller adapter is separate from raylib and uses the common `ps2::pad` encoder. Its mapping code is host-testable and is compiled into the Xbox smoke title. See [the port plan](PORT_PLAN.md) and [the smoke build instructions](../../port/xbox360/README.md).
