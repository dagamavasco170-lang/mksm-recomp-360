# Xbox 360 port — guest memory endian audit

## Confirmed source-level risk

The PS2 guest is little-endian. Xbox 360 Xenon PowerPC is big-endian. The current
runtime's scalar guest-memory helpers use `memcpy` into native `uint16_t`,
`uint32_t`, and `uint64_t` values in `ps2xRuntime/src/lib/ps2_memory.cpp`.
That is host-endian behavior and cannot be assumed correct on Xenon.

The direct `memcpy` paths are visible in the helper `loadScalar/storeScalar`
and are used by RDRAM, scratchpad and VU memory access paths. DMA and packed
structures also have direct memory copies and need a separate audit; not every
copy should be byte-swapped (byte arrays and payload copies must remain byte-exact).

## Required implementation sequence

1. Add portable explicit little-endian load/store primitives (this directory's
   `guest_endian.h`) and a small test that checks byte layouts and round-trips.
2. Refactor PS2Memory scalar accessors to use these primitives for guest scalar
   values, preserving byte-wise copies and device-register semantics.
3. Run the existing PS2 memory and runtime test suite on the desktop before porting.
4. Audit every direct native read of guest memory: instruction fetch, ELF parsing,
   DMA tags, GIF/VIF packets, packed structures, floating-point bit patterns.
5. Add Xenon tests for byte layout and address alignment, then verify the result
   on real RGH/JTAG hardware.

## Current status

- [x] Portable helper primitives and isolated smoke test added.
- [ ] Helpers integrated into PS2Memory.
- [ ] Desktop regression tests pass after integration.
- [ ] Xenon cross-build.
- [ ] Hardware validation.

No build or test was run in this environment. This document is a source-level
finding and implementation checklist, not a claim of validated behavior.
