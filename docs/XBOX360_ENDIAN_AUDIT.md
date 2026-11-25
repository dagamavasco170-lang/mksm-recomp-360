# Xbox 360 port — guest memory endian audit

## Confirmed source-level risk

The PS2 guest is little-endian. Xbox 360 Xenon PowerPC is big-endian. The scalar
guest-memory helpers previously used native-endian `memcpy` loads/stores in
`ps2xRuntime/src/lib/ps2_memory.cpp`, which could misinterpret PS2 values on Xenon.

Byte-array copies and packed/device payloads require individual review: not every
copy should be byte-swapped. Byte buffers must remain byte-exact.

## Implementation status

- [x] Added portable explicit little-endian primitive header and standalone tests under `xbox360/core-smoke`.
- [x] Integrated byte-wise little-endian `loadScalar/storeScalar` into `ps2_memory.cpp` for unsigned 16/32/64-bit scalar accesses to RDRAM, scratchpad and VU memory.
- [x] Focused host compiler check for scalar 16/32/64-bit read/write passed.
- [ ] Run full existing PS2 memory/runtime test suite after integration.
- [ ] Audit direct native guest-memory reads (instruction fetch, ELF parsing, DMA tags, GIF/VIF packets, packed structures and floating-point bit patterns).
- [ ] Xenon cross-build and hardware validation on RGH/JTAG.

The change preserves the scalar values and byte layout on little-endian hosts while
making the scalar helper interpretation independent of host byte order. It does not
prove the entire runtime Xenon-compatible; DMA, device-register semantics,
unaligned behavior, SIMD and concurrency remain separate audit areas.
