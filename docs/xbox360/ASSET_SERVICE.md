# Asset Service

Header: `port/xbox360/ps2_asset_service.h`

A runtime-facing handle API above RuntimeAssetResolver. `acquire(id)` resolves and loads according to the catalog, returns a typed handle, and increments reference counts for repeat acquisitions. `read(handle,offset,span)` reads cache/on-demand retained bytes or forwards stream reads. `release(handle)` decrements refs and removes the handle at zero. `clear()` resets service state.

Important: Cached blob pointers are borrowed from ResourceLoadingRuntime; cache eviction can invalidate them. This service is currently synchronous and not thread-safe, consistent with its underlying runtime. Do not use handles across runtime cache clear/eviction without adding pinning. Retail IDs are still intentionally not populated.
