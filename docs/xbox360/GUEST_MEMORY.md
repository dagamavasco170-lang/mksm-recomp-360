# Xbox 360 guest-memory and RDRAM address groundwork

## Why this exists

The PS2 EE memory model is little-endian. The Xbox 360 PowerPC CPU is big-endian. A target port must not reinterpret PS2 byte arrays as native PowerPC integers without an explicit endian conversion.

## Shared byte-order primitives

`port/xbox360/ps2_guest_memory.h` provides explicit little-endian reads and writes for unsigned 8-, 16-, 32- and 64-bit values, plus raw 16-byte copies that do not assign SIMD lane meaning. The wrapped forms are used for power-of-two RDRAM backing. Separate bounded forms are used for small regions such as scratchpad, where accesses must not wrap across the end of the region.

## Explicit EE address-region classification

`port/xbox360/ps2_rdram_address.h` exposes `classifyRegion(address)`. It identifies the following documented EE regions, including their KSEG0/KSEG1 physical aliases where applicable:

| Address range | Region | Current generated ABI support |
|---|---|---|
| `0x00000000-0x01FFFFFF`, `0x20000000-0x21FFFFFF`, `0x30100000-0x31FFFFFF` | Main RDRAM | Scalar 8/16/32/64-bit access |
| `0x70000000-0x70003FFF` | Scratchpad RAM (16 KiB) | Separate per-runtime scalar 8/16/32/64-bit access; compatibility pointer alias at `0xF0000000-0xF0003FFF` |
| `0x10000000-0x1000FFFF` | EE I/O registers (64 KiB) | Classified, no handlers |
| `0x11000000-0x11000FFF` | VU0 code memory (4 KiB) | Classified, no handlers |
| `0x11004000-0x11004FFF` | VU0 data memory (4 KiB) | Classified, no handlers |
| `0x11008000-0x1100BFFF` | VU1 code memory (16 KiB) | Classified, no handlers |
| `0x1100C000-0x1100FFFF` | VU1 data memory (16 KiB) | Classified, no handlers |
| `0x12000000-0x12001FFF` | GS privileged registers (8 KiB) | Classified, no handlers |
| `0x1C000000-0x1C1FFFFF` | IOP RAM (2 MiB) | Classified, no handlers |
| `0x1FC00000-0x1FFFFFFF` | BIOS (4 MiB) | Classified, no handlers |

Addresses in remaining KUSEG windows and KSSEG/KSEG3 are marked TLB-dependent rather than guessed. Unassigned holes in the KSEG0/KSEG1 direct physical aperture are marked unmapped. Scratchpad's canonical EE window is virtual-only and is not treated as an RDRAM or KSEG physical alias. For compatibility with the existing desktop `PS2Memory` pointer helpers, the generated ABI also recognizes the bit-31 selector alias `0xF0000000-0xF0003FFF`; this is a project compatibility mapping, not a second physical RDRAM window.

The region classification is metadata, not a claim that every device is emulated. The table and KSEG aliases are covered by `port/xbox360/tests/ps2_memory_region_test.cpp`. The map bases and sizes follow the [PS2 Developer Wiki EE memory map](https://www.psdevwiki.com/ps2/Memory_Map) and the [PS2Tek reference](https://psi-rockin.github.io/ps2tek/index.html).

## Main RDRAM windows

The scalar RDRAM resolver accepts only these windows:

| Guest virtual range | Backing RDRAM offset |
|---|---|
| `0x00000000-0x01FFFFFF` | `address` |
| `0x20000000-0x21FFFFFF` | `address - 0x20000000` (uncached alias) |
| `0x30100000-0x31FFFFFF` | `address - 0x30000000` (uncached accelerated alias) |
| KSEG0/KSEG1 (`0x80000000-0xBFFFFFFF`) | Low 29-bit physical address, accepted only when it falls inside the first 32 MiB of RDRAM |

This is a deliberately small fixed-window resolver, not a general EE TLB implementation. In particular, `0x02000000` is not treated as the documented uncached virtual alias; that alias begins at `0x20000000`.

## Scratchpad storage and scalar access

The generated ABI's `PS2Runtime` now owns an independently zero-initialized 16 KiB scratchpad buffer. Both the canonical `0x70000000` range and the project's bit-31 compatibility alias `0xF0000000` route to the same offsets. Generated `READ8/16/32/64`, `WRITE8/16/32/64` and corresponding `FAST_*` macros route addresses in `0x70000000-0x70003FFF` to that buffer, using explicit little-endian byte access. Scratchpad bytes do not alias the main RDRAM allocation.

Scratchpad accesses are bounded: a scalar operation whose final byte would exceed `0x70003FFF` throws `std::out_of_range` instead of wrapping to the start. A null runtime pointer also fails explicitly. The standalone ABI regression test exercises byte, halfword, word and doubleword access, end-of-region checks, RDRAM isolation, per-runtime isolation, and the same generated memory macros used by translated functions.

The scratchpad path is now also exercised by a second function generated from the same synthetic little-endian MIPS ELF. This 15-instruction function executes real `SW/LW`, `SB/LBU`, `SH/LHU` and `SD/LD` instructions through PS2Recomp-generated C++, accesses both `0x70000000` and the existing project compatibility alias `0xF0000000`, and writes a separate word to verify that scratchpad does not alias main RDRAM. The Xenon-ABI host runner reports:

```text
MKSM_RECOMP_MEMORY_TRACE failures=00000000 ... spw=A1B2C3D4 spb=5A sph=BEEF spd=0123456789ABCDEF aliasw=A1B2C3D4 aliasb=C7 isolated=DEADBEEF
```

The XEX workflow compiles the exact same generated functions and calls the probe during smoke-title startup. This is still host execution evidence plus a PowerPC build: the current XEX must be launched on an RGH/JTAG console to verify the scratchpad trace on physical Xenon hardware.

The generated ABI now routes aligned 32-bit accesses in the EE I/O window through a per-runtime register bank for the two normal-mode SPR DMA channels. Channel `0x1000D000` copies scratchpad to RDRAM; channel `0x1000D400` copies RDRAM to scratchpad. The implementation follows the desktop `PS2Memory` path and its `spr_dma_regression` cases for QWC sizing, 16 KiB SADR wrap, MADR alignment/bit-31 masking, completion bits and stall-control D_STADR updates. It rejects chain/interleave/MFIFO mode and invalid memory ranges without reporting completion. Host tests cover three MADR aliases, both directions, QWC cases and D_STAT W1C/mask behavior. A 57-word PS2Recomp-generated MIPS function also programs the channels using guest `SW`, starts both directions and checks completion/copy data using guest `LW`; the generated function is linked into the smoke XEX and emits `MKSM_SPR_DMA_GENERATED_TRACE`.

This remains a synchronous normal-mode model, not cycle-accurate DMA timing or channel arbitration. Other DMAC channels, chain/MFIFO/interleave modes, cache behavior and device-specific MMIO are still unimplemented. The desktop `PS2Memory` subsystem and full Xenon runtime are not yet unified behind this shim.

## PS2Recomp-generated execution probe

The synthetic PS2Recomp fixture emits `SB/SH/SW/SD`, `LB/LBU/LHU/LW/LD`, arithmetic and a taken branch with a delay slot. Its `SW` targets the final aligned RDRAM word at `0x01FFFFF8`. A separate base register then targets `0x20000000`, the documented uncached alias: aligned `SD/LD` access backing offset zero, and byte/halfword operations verify additional bytes through that same alias.

The 23-instruction generated RDRAM function runs in CI against both the desktop reference runtime and the Xbox generated-runtime ABI shim on the host. A second 15-instruction PS2Recomp function now exercises scratchpad through generated `SW/LW/SB/LBU/SH/LHU/SD/LD` instructions, including the bit-31 compatibility alias; the Xenon-ABI host runner reports zero failures and the expected values in the trace above. The smoke XEX compiles both generated functions and invokes the same memory probe at title startup. Its execution on a physical RGH/JTAG console remains the next acceptance gate.

## Important scope limits

This is **not** the full PS2 memory subsystem. The generated-code shim currently supports scalar 8/16/32/64-bit RDRAM and scratchpad operations through the explicit paths above.

It does not yet implement:
- General TLB / COP0 translation or all possible virtual aliases.
- Non-SPR DMA channels, chain/interleave/MFIFO modes, cycle timing and arbitration.
- VU memories, GS VRAM, IOP RAM or device MMIO handlers beyond the two normal SPR channels.
- 128-bit LQ/SQ and vector memory lane representation.
- DMA, cache behavior, atomics or concurrent guest-memory access.
- A full integration with the desktop `PS2Memory` class or a complete Xenon `PS2Runtime`.

Unsupported generated operations must remain compile-time or runtime failures until they have dedicated semantic tests. Do not enable the retail game translation with this shim yet.
