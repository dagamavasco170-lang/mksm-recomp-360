# Technical references and reuse boundaries

The Shaolin Monks Xbox 360 port uses two complementary public projects as engineering references. They are references for architecture, investigations and test methodology—not a substitute for implementing and validating the game's own PS2 behavior.

## Reference chain

- [Tenkaichi3Decomp](https://github.com/z3xox/Tenkaichi3Decomp): PC recompilation/decompilation-based reconstruction of Budokai Tenkaichi 3. Study how game code, platform dependencies, and generated/runtime components are organized.
- [BT3XBx](https://github.com/Lucas-Tito/bt3xbx): native Xbox 360 port of BT3, forked from Tenkaichi3Decomp. Study Xbox-specific platform separation, PowerPC/Xenos build and packaging, input, rendering, audio, memory/system services, hardware constraints, and acceptance/testing practices.

## How to apply lessons to MKSM

| Area | Tenkaichi3Decomp | BT3XBx | MKSM implementation |
|---|---|---|---|
| Decompiled/game logic | Organization and reconstruction patterns | Platform boundary around game code | Preserve our own PS2 recompilation and game logic |
| Runtime / EE | PS2 behavior and PC runtime reference | Xbox-specific runtime/platform lessons | Implement and test MKSM-specific EE/IOP semantics |
| SIMD/MMI/VU | Semantics and host implementation context | Xenon target constraints | Independent scalar references, differential tests, then PPC/VMX backend |
| Graphics | Game-side graphics structure | Native Xenos rendering/platform integration | GS-to-Xenos adapter with image-based tests |
| Input/audio/system | Host-side abstractions | Console APIs and target constraints | Isolated Xbox adapters |
| Build/release | Host build/recompilation workflow | XEX packaging and hardware validation | Reproducible CI plus console evidence |

## Engineering workflow

1. Inspect the relevant source and documentation in the reference project.
2. Write down the applicable concept and the exact MKSM blocker it addresses.
3. Prefer architecture and test patterns; do not transplant code blindly.
4. Verify source license, file-level notices, dependencies, and compatibility before copying or adapting any code.
5. Implement an isolated change in this repository.
6. Add game-data-free tests and host differential tests where applicable.
7. Build the Xbox target in CI; run on hardware when the acceptance criterion requires it.
8. Record exactly what the test demonstrates and what remains unverified.

## License awareness

The repository licenses are not interchangeable: Tenkaichi3Decomp is marked AGPL-3.0, while BT3XBx's top-level license is MIT. The MIT license of the Xbox port does not automatically relicense code or dependencies inherited from its upstream. Before copying or adapting source code, inspect the relevant file's history, headers, upstream origin and applicable license obligations. This document adopts both projects as research references; it does not authorize copying code or game assets.

## Current priority

Use the references to guide the next independent MKSM engineering step: strengthen the guest-state and MMI semantics (especially 128-bit HI/LO and PMADDH/PMADDW), build host-side differential tests, and only then integrate the validated operation into the runtime/code generator. Do not mistake a smoke XEX for game execution.
