# Xbox 360 backend

This directory is reserved for the native Xbox 360 backend of MKSM Recomp.

## Current state

**Scaffolding only. No Xbox 360 executable or working backend exists yet.**

The first implementation must pass the toolchain feasibility gate and produce a minimal test XEX before game runtime code is ported.

## Planned layout

- `cmake/` — Xbox-only toolchain and target configuration, once validated.
- `src/platform/` — memory, time, threads, input, files and logging adapters.
- `src/graphics/` — Xenos/Direct3D 9 implementation of the GS backend.
- `src/audio/` — Xbox audio output and streaming adapters.
- `tools/` — packaging and metadata generation scripts that do not include proprietary SDK files.
- `tests/` — game-data-free target-independent checks.

Do not add empty implementation files or pretend stubs are functional. Add each component only with a clear interface, a test and a documented status.

## Build prerequisites

A licensed Xbox 360 development toolchain is required. It is not downloaded, vendored or distributed by this repository. See [the port plan](../../docs/xbox360/PORT_PLAN.md) for the feasibility gate and milestones.
