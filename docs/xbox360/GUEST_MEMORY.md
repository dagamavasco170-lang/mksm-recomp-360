# Xbox 360 guest-memory endianness groundwork

## Why this exists

The PS2 EE memory model is little-endian. The Xbox 360 PowerPC CPU is big-endian. The desktop runtime's fast helpers in `ps2_runtime_macros.h` use `memcpy` between guest RAM bytes and native integer values; that produces the intended values on the current x86 desktop host, but cannot be assumed to do so on a big-endian PowerPC host.

A target port must not reinterpret PS2 byte arrays as native PowerPC integers without an explicit endian conversion.

## Shared byte-order primitive

`port/xbox360/ps2_guest_memory.h` provides explicit little-endian reads and writes for unsigned 8-, 16-, 32- and 64-bit values, masked wrapping for power-of-two buffers, and raw 16-byte byte copies that do not assign SIMD lane meaning. It rejects empty and non-power-of-two buffers.

The original smoke test exercises a small wrapped buffer. The helper is now also wired into `ps2_generated_runtime_abi.h`, which supplies the generated-code macros `READ8/16/32/64` and `WRITE8/16/32/64` over a fixed 32 MiB RDRAM view. This keeps scalar guest accesses little-endian on both little-endian hosts and big-endian Xenon.

## Generated-code memory probe

The PS2Recomp fixture now emits a synthetic function with `SB/SH/SW/SD`, `LB/LBU/LHU/LW/LD`, arithmetic and a taken branch with a delay slot. Its word store begins at guest address `0x01FFFFFF`, so the four bytes cross the 32 MiB boundary and must wrap. The same generated function is executed twice in CI: once against the desktop runtime and once against the Xbox generated-runtime ABI shim on the host. The Xbox smoke XEX also compiles and executes this exact generated function at title startup.

The host ABI runner checks loaded register values and the exact bytes written at the end and beginning of RDRAM. The XEX logs `MKSM_RECOMP_XENON_TRACE` with the generated register and memory results. A host pass validates the shim's byte-order logic; it does not replace the required physical-console run.

## Important scope limits

This is **not** the full PS2 memory subsystem. The generated-code shim supports only scalar 8-, 16-, 32- and 64-bit RDRAM accesses in the simple physical and KSEG address windows, including the 32 MiB RDRAM mask/wrap behavior.

It deliberately rejects known I/O, VU, GS and BIOS physical regions instead of silently aliasing them into RDRAM. It does not yet implement:
- Full TLB and virtual/physical address translation or all address aliases.
- Scratchpad, VU memories, GS VRAM or memory-mapped I/O handlers.
- 128-bit LQ/SQ and vector memory lane representation.
- DMA, cache behavior, atomics or concurrent guest-memory access.
- A full integration with the desktop `PS2Memory` class or a complete Xenon `PS2Runtime`.

Unsupported generated operations must remain compile-time or runtime failures until they have dedicated semantic tests. Do not enable the retail game translation with this shim yet.
