# Xbox 360 SIMD compatibility audit

## Finding

The desktop runtime and recompiler are not yet source-compatible with the 32-bit big-endian PowerPC Xbox 360 target. The smoke executable deliberately excludes the desktop PS2 runtime and the host-side recompiler, so a successful smoke build does not prove either component can compile for Xenon.

A targeted source scan of 11 production files found **611 textual occurrences of _mm_* identifiers across 99 distinct names**. Three of those names are project-local helper wrappers (_mm_custom_sllv_epi32, _mm_custom_srlv_epi32, _mm_custom_srav_epi32), not standard Intel intrinsics. The scan also found __m128i and __m128 vector types in runtime and recompiler paths. These are source-text counts, not dynamic instruction counts: they include macro definitions and do not imply that all operations execute in a game frame.

The dependency spans both sides of the build:
- Runtime macros and state/memory paths contain PS2 MMI packed integer operations, VU float helpers, and vector load/store/state handling.
- The host recompiler's MMI and VU translation helpers also use SSE operations. Porting only the runtime would leave the generator itself dependent on x86.
- The Xbox 360 target is big-endian PowerPC. x86 vector types and lane ordering cannot be assumed to map directly to PS2 guest little-endian lane semantics or Xenon VMX128.

The counts are a targeted lower bound, not a claim that every repository source file, third-party dependency, generated translation or emitted code path has been exhaustively audited.

## Targeted inventory

| Production source | _mm_* textual occurrences | Main area observed |
|---|---:|---|
| ps2xRuntime/include/ps2_runtime_macros.h | 167 | Runtime MMI integer macros and VU float macros |
| ps2xRuntime/include/ps2_runtime.h | 9 | CPU/VU vector state, lane extraction and 64-bit register construction |
| ps2xRuntime/src/lib/ps2_runtime.cpp | 36 | Runtime vector state, float bitcasts, loads/stores and lane extraction |
| ps2xRuntime/src/lib/ps2_memory.cpp | 9 | 128-bit memory read/write paths |
| ps2xRuntime/src/lib/ps2_vif1_interpreter.cpp | 2 | 128-bit vector loads/stores |
| ps2xRecomp/src/lib/mmi_translation_helpers.cpp | 163 | MMI translation helpers: saturation, arithmetic, multiply, shuffle, pack/unpack |
| ps2xRecomp/src/lib/mmi_translator.cpp | 6 | MMI shift translation |
| ps2xRecomp/src/lib/vu_translation_helpers.cpp | 196 | VU four-lane float operations, conversion, shuffle/blend and casts |
| ps2xRecomp/src/lib/vu_translator.cpp | 19 | VU translate-time vector construction, blend, shuffle and casts |
| ps2xRecomp/src/lib/instruction_translator.cpp | 2 | Vector bitcasts in instruction translation |
| ps2xRecomp/include/ps2recomp/codegen_helpers.h | 2 | Shared vector construction/cast helpers |
| **Total** | **611** | **11 production files** |

The 99 distinct _mm_*-prefixed names are counted over the first ten files; the two extra helper names in codegen_helpers.h are already present in that set. An additional single _mm_blendv_ps reference occurs in ps2xTest/src/code_generator_tests.cpp and is test-only, so it is not included in the production total.

## Scalar reference coverage

The game-data-free scalar reference is implemented in port/xbox360/ps2_simd_scalar.h. It stores bytes in PS2 guest-address order and defines lane zero as the least-significant lane, independent of host endianness.

Implemented and host-tested operations:
- Lane reads/writes for unsigned 8-, 16- and 32-bit lanes.
- Wrapping add/subtract for unsigned 8-, 16- and 32-bit lanes.
- Bitwise AND, OR, XOR and NOR.
- Equality masks and signed greater-than comparisons for 8-, 16- and 32-bit lanes.
- Signed min/max and two's-complement absolute value for 8-, 16- and 32-bit lanes.
- Signed saturating 32-bit add/subtract using widened scalar intermediates.
- Variable 32-bit logical left, logical right and arithmetic right shifts; counts follow the helper's low-five-bit rule.
- Saturating 32-to-16-bit packing.
- Even-lane packing (PPACB/H/W reference ordering) and low/high interleave (PEXT* reference ordering).

The host tests check selected normal and boundary values. The run #80 Xbox smoke title executes a smaller scalar packed-vector probe, and the user-provided console log confirms that probe passed on hardware. There is not yet a broad differential suite comparing every scalar primitive against the current x86 implementation.

## Gaps that remain

The scalar layer is not a drop-in replacement for all 99 source symbols. Important missing areas include:
- Signed/unsigned saturating 8- and 16-bit arithmetic.
- Packed multiply and multiply-accumulate variants.
- Byte/word/dword shuffle controls and byte-mask blend semantics.
- 64-bit lane operations, vector insert/extract variants and complete load/store adapters.
- VU floating-point arithmetic, conversions, rounding, denormal/NaN behavior and status/flag semantics.
- An explicit mapping of runtime register structs and generated-code state onto guest-order bytes.

Do not translate names mechanically. For each operation, record its PS2 instruction semantics, current SSE implementation behavior, guest lane order, signedness, overflow/saturation rules, shift-count behavior, alignment, and any observable floating-point flags.

## Next work

1. Build operation-by-operation MMI and VU coverage tables from the exact translation helpers and runtime macros, distinguishing standard intrinsics from project-local wrappers.
2. Add host differential tests comparing scalar primitives with the current SSE reference on deterministic vectors and edge cases. Keep this reference path unchanged.
3. Extend scalar MMI operations in small, tested groups: saturation, multiply/accumulate, shuffle/pack/unpack and 64-bit helpers.
4. Treat VU floating-point as a separate semantic port; do not assume ordinary C++ float operations exactly match PS2 VU behavior.
5. Only after the scalar semantics are validated, design the Xbox-specific backend and validate the target ABI and Xenon VMX128 instruction set. Do not assume desktop AltiVec is equivalent.
6. Migrate the recompiler and runtime independently behind explicit platform abstractions, then test the same deterministic vectors on host and console.

## Related work

The XAM controller adapter, guest-memory little-endian helper and the selected scalar SIMD probe are compiled into the Xbox smoke title. The user's run #80 hardware log confirms the current smoke probe passed on the console. See PORT_PLAN.md, GUEST_MEMORY.md and HARDWARE_SMOKE_2026-11-22.md.
