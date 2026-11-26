# AFS Resource Runtime Bridge

Header `port/xbox360/ps2_resource_afs_bridge.h` joins `AfsStreamIo` and `ResourceLoadingRuntime`.

- `loadBlob(entry, out)`: opens virtual AFS entry, determines size, reads full payload, closes descriptor.
- `readChunk(entry, offset, output)`: opens entry, seeks to requested offset, reads only the supplied buffer, closes descriptor.
- `loader(entry)` and `streamReader(entry)`: callback adapters for cache and streaming runtime APIs.

The bridge is archive-scoped and currently synchronous. It uses the existing AFS path and IOMAN-compatible stream subsystem, but is not yet wired to game-generated resource IDs or texture/audio consumers. It does not embed game data.
