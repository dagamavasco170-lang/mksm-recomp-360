# Targeted SIMD source inventory — 2026-11-22

This inventory records the focused source scan that expanded the original audit. It is deliberately scoped to the primary PS2 runtime and host recompiler files; it is not an exhaustive repository-wide scan.

## Summary

- 11 production files inspected for _mm_* identifiers.
- 611 textual occurrences of _mm_* identifiers.
- 99 distinct _mm_*-prefixed names across the first ten files (three are project-local custom helper wrappers rather than standard Intel intrinsics).
- A new scalar MMI saturation slice now covers signed and unsigned saturating add/subtract for 8-, 16- and 32-bit lanes.
- __m128i and __m128 types appear in runtime and recompiler source.
- One additional _mm_blendv_ps reference is present in a test file and excluded from production totals.

These figures count source text, including macro definitions; they are not runtime execution counts.

## Per-file counts

| File | Occurrences | Area |
|---|---:|---|
| ps2xRuntime/include/ps2_runtime_macros.h | 167 | MMI and VU runtime macros |
| ps2xRuntime/include/ps2_runtime.h | 9 | Runtime vector state |
| ps2xRuntime/src/lib/ps2_runtime.cpp | 36 | State movement, casts and vector loads/stores |
| ps2xRuntime/src/lib/ps2_memory.cpp | 9 | 128-bit guest-memory accesses |
| ps2xRuntime/src/lib/ps2_vif1_interpreter.cpp | 2 | Vector loads/stores |
| ps2xRecomp/src/lib/mmi_translation_helpers.cpp | 163 | Packed integer MMI helpers |
| ps2xRecomp/src/lib/mmi_translator.cpp | 6 | MMI shift helpers |
| ps2xRecomp/src/lib/vu_translation_helpers.cpp | 196 | VU float and conversion helpers |
| ps2xRecomp/src/lib/vu_translator.cpp | 19 | VU vector construction/shuffle/blend |
| ps2xRecomp/src/lib/instruction_translator.cpp | 2 | Vector bitcasts |
| ps2xRecomp/include/ps2recomp/codegen_helpers.h | 2 | Shared code-generation vector helpers |
| **Total** | **611** | |

Test-only reference (excluded): ps2xTest/src/code_generator_tests.cpp has one _mm_blendv_ps occurrence.

## Architectural implication

There are at least two independent x86 portability fronts:

1. **Build-host recompiler:** MMI and VU translation helpers use SSE types and operations while generating/translating game instructions. This code must remain on the host or be refactored into portable code; it should not be built with the PowerPC target toolchain merely because the generated title is for Xbox 360.
2. **Xbox runtime:** runtime macros, state, memory and VIF paths use vector types and intrinsics. These need a platform-neutral semantic layer and a separately validated Xenon implementation.

The existing Xbox smoke project compiles neither the complete recompiler nor the full PS2 runtime. Its successful XEX build therefore validates only the isolated smoke target.

## Immediate follow-up

- Create a mapping from the exact operations in mmi_translation_helpers.cpp, vu_translation_helpers.cpp and ps2_runtime_macros.h to the PS2 instruction semantics and current desktop reference behavior.
- Add deterministic scalar-versus-SSE differential tests before replacing any implementation.
- Prioritize MMI integer operations separately from VU floating-point semantics.
- Keep generated game code and proprietary game data out of the audit artifact.


## Follow-on scalar MMI saturation slice

The Xbox branch now includes generic scalar saturating add/subtract helpers for signed and unsigned 8-, 16- and 32-bit lanes. The host unit tests exercise byte/halfword signed boundaries and byte/word unsigned overflow/underflow. The SSE2 differential test compares:

- Signed byte saturation against _mm_adds_epi8 / _mm_subs_epi8.
- Signed halfword saturation against _mm_adds_epi16 / _mm_subs_epi16.
- Unsigned byte saturation against _mm_adds_epu8 / _mm_subs_epu8.
- Unsigned word saturation against the same biased signed-compare reference used by the current runtime helpers.

The XEX smoke probe additionally checks signed byte saturation and unsigned byte/word saturation on target. A new CI run and physical console retest are still required for this expanded set.
