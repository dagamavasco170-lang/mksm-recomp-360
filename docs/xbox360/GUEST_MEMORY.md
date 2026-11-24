# Xbox 360 guest-memory and RDRAM address groundwork

## Why this exists

The PS2 EE memory model is little-endian. The Xbox 360 PowerPC CPU is big-endian. A target port must not reinterpret PS2 byte arrays as native PowerPC integers without an explicit endian conversion.

## Shared byte-order primitive

`port/xbox360/ps2_guest_memory.h` provides explicit little-endian reads and writes for unsigned 8-, 16-, 32- and 64-bit values, plus raw 16-byte copies that do not assign SIMD lane meaning. It rejects empty and non-power-of-two buffers.

The generated-code ABI uses these helpers over a fixed 32 MiB RDRAM buffer. It does not depend on the host's native byte order.

## Explicit EE address-region classification

`port/xbox360/ps2_rdram_address.h` now exposes `classifyRegion(address)`. It identifies the following documented EE regions, including their KSEG0/KSEG1 physical aliases where applicable:

| Address range | Region | Classification only |
|---|---|---|
| `0x00000000-0x01FFFFFF`, `0x20000000-0x21FFFFFF`, `0x30100000-0x31FFFFFF` | Main RDRAM | Backed by the current scalar ABI |
| `0x70000000-0x70003FFF` | Scratchpad RAM (16 KiB) | Not implemented |
| `0x10000000-0x1000FFFF` | EE I/O registers (64 KiB) | Not implemented |
| `0x11000000-0x11000FFF` | VU0 code memory (4 KiB) | Not implemented |
| `0x11004000-0x11004FFF` | VU0 data memory (4 KiB) | Not implemented |
| `0x11008000-0x1100BFFF` | VU1 code memory (16 KiB) | Not implemented |
| `0x1100C000-0x1100FFFF` | VU1 data memory (16 KiB) | Not implemented |
| `0x12000000-0x12001FFF` | GS privileged registers (8 KiB) | Not implemented |
| `0x1C000000-0x1C1FFFFF` | IOP RAM (2 MiB) | Not implemented |
| `0x1FC00000-0x1FFFFFFF` | BIOS (4 MiB) | Not implemented |

Addresses in remaining KUSEG windows and KSSEG/KSEG3 are marked TLB-dependent rather than guessed. Unassigned holes in the KSEG0/KSEG1 direct physical aperture are marked unmapped. The classifier is metadata only: it does not provide reads, writes, device side effects, DMA, or TLB translation.

The table and KSEG aliases are covered by `port/xbox360/tests/ps2_memory_region_test.cpp`. The existing `ps2_rdram_address_test.cpp` continues to verify accepted RDRAM backing offsets and that every non-RDRAM region throws when passed to the scalar RDRAM resolver. The map sizes and bases follow the [PS2 Developer Wiki EE memory map](https://www.psdevwiki.com/ps2/Memory_Map) and the [PS2Tek reference](https://psi-rockin.github.io/ps2tek/index.html).

## Explicit EE RDRAM virtual windows

The scalar resolver still accepts only these RDRAM windows:

| Guest virtual range | Backing RDRAM offset |
|---|---|
| `0x00000000-0x01FFFFFF` | `address` |
| `0x20000000-0x21FFFFFF` | `address - 0x20000000` (uncached alias) |
| `0x30100000-0x31FFFFFF` | `address - 0x30000000` (uncached accelerated alias) |
| KSEG0/KSEG1 (`0x80000000-0xBFFFFFFF`) | Low 29-bit physical address, accepted only when it falls inside the first 32 MiB of RDRAM |

This is a deliberately small fixed-window resolver, not a general EE TLB implementation. In particular, `0x02000000` is not treated as the documented uncached virtual alias; that alias begins at `0x20000000`. Unsupported regions throw descriptive exceptions instead of being silently folded into RDRAM.

## PS2Recomp-generated execution probe

The synthetic PS2Recomp fixture emits `SB/SH/SW/SD`, `LB/LBU/LHU/LW/LD`, arithmetic and a taken branch with a delay slot. Its `SW` targets the final aligned RDRAM word at `0x01FFFFF8`. A separate base register then targets `0x20000000`, the documented uncached alias: aligned `SD/LD` access backing offset zero, and byte/halfword operations verify additional bytes through that same alias.

The exact generated function runs in CI against both the desktop reference runtime and the Xbox generated-runtime ABI shim on the host. The smoke XEX also compiles and executes this function at title startup. The fixture uses naturally aligned multi-byte operations; it does not rely on unaligned `SW/SD` behavior or claim that `0x02000000` is a virtual alias.

The code-generation test obtains the callable entry name from PS2Recomp's emitted function header, instead of assuming that the output filename and C++ symbol use the same suffixing convention.

A host pass validates the resolver and byte-order logic, but it does not replace the required physical-console run.

## Important scope limits

This is **not** the full PS2 memory subsystem. The generated-code shim currently supports only scalar 8-, 16-, 32- and 64-bit RDRAM operations through the explicit windows above.

It does not yet implement:
- General TLB / COP0 translation or all possible virtual aliases.
- Scratchpad storage and its DMA access path.
- VU memories, GS VRAM, IOP RAM or memory-mapped I/O handlers.
- 128-bit LQ/SQ and vector memory lane representation.
- DMA, cache behavior, atomics or concurrent guest-memory access.
- A full integration with the desktop `PS2Memory` class or a complete Xenon `PS2Runtime`.

Unsupported generated operations must remain compile-time or runtime failures until they have dedicated semantic tests. Do not enable the retail game translation with this shim yet.
