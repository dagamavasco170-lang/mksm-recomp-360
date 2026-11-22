# Xbox 360 SIMD compatibility audit

## Finding

The desktop runtime is not yet source-compatible with the PowerPC Xbox 360 compiler. The smoke executable does not include or link the PS2 runtime, so its successful build must not be interpreted as proof that the complete runtime can compile.

A targeted inventory found 71 distinct _mm_* operation names (167 references) in ps2_runtime_macros.h alone, plus further SSE use in ps2_runtime.h, ps2_runtime.cpp and ps2_memory.cpp. This is not an exhaustive scan of every runtime translation unit or generated game unit.

The existing code uses x86 intrinsic types, headers and semantics for two different PS2 workloads: packed integer MMI operations and four-lane floating-point VU helpers. The Xbox 360 target is 32-bit big-endian PowerPC; the current x86 vector types cannot be assumed to exist or preserve guest lane order there.

## First scalar reference slice

The new port/xbox360/ps2_simd_scalar.h introduces a game-data-free 128-bit scalar reference representation. It stores bytes in PS2 guest-address order and defines lane zero as the least-significant lane, independent of the host CPU's native endianness.

The initial tested subset contains:
- Wrapping add and subtract for unsigned 8-, 16- and 32-bit lanes.
- Bitwise AND, OR, XOR and NOR.
- Equality comparison masks (all ones for equal lanes, zero otherwise).
- Low/high lane interleave in the same argument order as the current PS2_PEXT* SSE macros.

Host unit tests cover arithmetic wraparound, logical masks, lane equality, byte order and interleave order. The Xbox smoke target also compiles and executes a small scalar-vector probe so the same reference code is exercised by the pinned PowerPC toolchain and physical smoke run.

## Important limits

This is only a scalar reference slice. It does not yet implement saturating arithmetic, signed comparisons, packing, shuffle controls, variable shifts, multiply/accumulate, 64-bit operations or VU floating-point semantics. It is not yet connected to the desktop SSE backend, the generated code, PS2 runtime state or PS2Memory. No optimized Xenon VMX128 backend is claimed.

## Next work

1. Expand the exhaustive inventory across runtime, IOP, GS, VU, generated-code templates and emitted code. Record signedness, lane width/order, saturation, shift rules and alignment assumptions.
2. Extend the scalar reference and tests operation-by-operation, including saturation, packing, shuffles, shifts, multiply/accumulate and float bitcasts.
3. Add desktop differential tests against the current SSE implementation while keeping the desktop runtime unchanged.
4. Implement a separate Xbox backend only after the scalar semantics are fully covered; verify Xenon-specific instructions and ABI rather than assuming ordinary AltiVec matches VMX128.
5. Migrate runtime structures and generated game code only after deterministic vectors agree across reference and target paths.

## Initial inventory (not exhaustive)

| Source | Observed SIMD use |
|---|---|
| ps2_runtime_macros.h | 167 SSE-operation references across 71 distinct intrinsic names; PS2 MMI integer macros and VU float macros |
| ps2_runtime.h | __m128i / __m128 CPU/VU state, lane extraction and 64-bit register construction |
| ps2_runtime.cpp | vector state save/restore, float bitcasts, lane extraction and vector loads/stores |
| ps2_memory.cpp | 128-bit read/write paths over RDRAM, scratchpad and VU memory |

The targeted inventory also inspected ps2_vif1_interpreter.cpp. The counts are lower bounds, not a complete repository-wide total.

## Related work

The separate XAM controller adapter and explicit little-endian guest-memory groundwork remain host-tested and are compiled into the smoke title. See PORT_PLAN.md and GUEST_MEMORY.md.
