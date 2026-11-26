# Resource Loading Runtime

Portable C++20 header-only primitives for runtime resource cache/streaming. No title assets are bundled.

Header: `port/xbox360/ps2_resource_loading_runtime.h`

- `loadCached(id, loader)`: full-load into byte vector; bounded LRU cache, cache hit/miss and eviction counters. Refused if the object exceeds budget.
- `loadOnDemand(id, loader, out)`: transient load; caller owns the resulting vector and runtime does not retain it.
- `stream(id, offset, output, reader)`: bounded caller-buffer streaming interface; validates reader byte count and tracks streamed bytes.
- `state(id)`, `stats()`, `setCacheBudget()`, `erase()`, `clear()`

The loader and stream-reader callbacks are the integration seams for IOMAN/AFS. A returned cache blob pointer is invalidated by eviction or clear; callers must not retain it across cache-mutating calls. Current implementation is synchronous and is not yet thread-safe. Cache budgets are explicit; no default budget assumes measured title RAM. Xbox hardware profiling is still required for timing and memory validation.
