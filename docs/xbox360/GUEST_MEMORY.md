# Xbox 360 guest-memory endianness groundwork

## Why this exists

The PS2 EE memory model is little-endian. The Xbox 360 PowerPC CPU is big-endian. The desktop runtime's fast helpers in `ps2_runtime_macros.h` use `memcpy` between guest RAM bytes and native integer values; that produces the intended little-endian values on the current x86 desktop host, but cannot be assumed to do so on a big-endian PowerPC host.

A target port must not reinterpret PS2 byte arrays as native PowerPC integers without an explicit endian conversion.

## First isolated helper

`port/xbox360/ps2_guest_memory.h` currently provides:

- Explicit little-endian reads and writes for unsigned 8-, 16-, 32- and 64-bit values.
- Masked wrapping for a power-of-two RDRAM test buffer.
- Raw 16-byte reads/writes that preserve byte order rather than assigning host SIMD lane meaning.
- Rejection of empty and non-power-of-two buffers.

The Xbox smoke source performs a target-side read test across the end of a 16-byte probe buffer. GitHub Actions run #76 passed the host-side test covering normal reads, wraparound reads, writes, raw 128-bit byte order and invalid buffer rejection, and compiled the target probe into the XEX. The revised probe still needs execution on the physical console.

## Important scope limits

This is a **small, standalone endian helper**, not the PS2 memory subsystem port. It is not yet wired into `PS2Memory`, generated-code memory macros or the existing `Ps2FastRead*/Ps2FastWrite*` helpers.

It does not implement:
- PS2 virtual/physical address translation.
- 32 MiB RDRAM allocation or aliasing.
- Scratchpad, VU memories, GS VRAM or memory-mapped I/O.
- DMA, cache behavior, atomics or concurrent guest-memory access.
- The 128-bit MMI/VU vector lane representation.
- Any change to the existing PC backend.

The next engineering step is to use these explicit byte-order semantics as the reference when designing the shared `ps2_simd` and guest-memory abstractions. Tests must compare the new PowerPC behavior with the existing desktop behavior before runtime integration.
