# External port/recompilation references

This port uses publicly available projects as **engineering references**, not as a source of proprietary game files. References are evaluated for architecture, tooling, validation practices, and reusable code whose license and provenance are compatible.

## OpenShao (Xbox static recompilation)

- Repository: https://github.com/OAleex/OpenShao
- License reported by the repository: GPL-2.0
- Game input: user's own Xbox copy; repository README says it does not include the game.

**How we use it as a reference**
- Study its static-recompilation pipeline, input-file validation and separation between generated code, runtime, and user-provided game data.
- Compare Xbox executable loading/metadata and generated-code testing approaches where relevant.
- Use it as a reference for build UX, diagnostics, file validation, and packaging conventions.

**Important architecture boundary:** OpenShao recompiles the original Xbox version. This project targets the PS2 release (MIPS R5900/Emotion Engine, IOP, GS/VU), so its Xbox-specific recompilation output, CPU semantics, graphics backend, and game-specific assumptions are not drop-in replacements. Any source code reuse must be checked file-by-file for license compatibility, attribution, dependencies, and platform suitability.

## OGmidway MKSM Recomp (PS2)

- Repository: https://github.com/OGmidway/mksm-recomp
- Its README describes an experimental PS2 retail recompilation, with runtime, analyzer, recompiler, IOP, tests, and contributor tools. It requires users to supply their own retail game files.

**How we use it as a reference**
- Prioritize shared PS2-specific research: EE/IOP behavior, retail file loading, RPC/IOP service boundaries, function analysis, tests, and game-data extraction/build workflow.
- Compare findings and tests before implementing a parallel feature; preserve a clear attribution trail for any compatible code that is reused.

## Reference-use policy for this repository

1. Link and document upstream sources and their licenses.
2. Prefer adapting documented concepts and independently implemented behavior over copying code.
3. Before copying any code, confirm the exact license of that file and its dependencies, preserve copyright/license notices, and record origin in `docs/CREDITS.md`.
4. Never assume a repository's license covers original game executables, disc images, media, or assets.
5. Keep original game data outside Git; build scripts should accept a local user-supplied disc/extraction directory.

## Relevance to the Xbox 360 target

- **Directly relevant:** runtime/build separation, reproducible generation, tests, input-file checks, artifact packaging, diagnostics.
- **PS2-specific reference:** OGmidway's PS2 recomp/runtime and IOP research.
- **Indirect reference only:** OpenShao's Xbox recompilation architecture and validation workflow; its Xbox binary translation is not equivalent to recompiling the PS2 binary for Xenon.
