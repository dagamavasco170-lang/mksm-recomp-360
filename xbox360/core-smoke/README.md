# Guest endian smoke test

This is a small, dependency-free portability test for the PS2 guest-memory byte order.
It is intentionally isolated from the desktop runtime and does not contain game assets.

The PS2 guest memory is little-endian. Xbox 360 Xenon is PowerPC big-endian, so
host-native multi-byte reads/writes must not be used to interpret guest bytes.

## Build and run on a host

```sh
cmake -S xbox360/core-smoke -B build/guest-endian
cmake --build build/guest-endian
ctest --test-dir build/guest-endian --output-on-failure
```

The test covers 16-, 32-, and 64-bit explicit little-endian loads/stores, plus
unaligned 32-bit access.

**Scope:** these helpers are not yet wired into `PS2Memory`. The existing runtime's
`read16/read32/read64` and `write16/write32/write64` paths still require a
careful refactor and regression tests. This smoke test only validates the portable
helper primitives; it does not validate the Xbox SDK, XEX, or hardware.
