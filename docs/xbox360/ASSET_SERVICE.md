# Asset Service

Header: `port/xbox360/ps2_asset_service.h`

A runtime-facing handle API above RuntimeAssetResolver. `acquire(id)` resolves metadata first, eagerly loads Cache and OnDemand entries, and creates handles for Stream/SeekAware entries without forcing a full load. Repeat acquisitions reuse a handle and increment its reference count. `read(handle,offset,span)` copies from retained OnDemand bytes, routes Cache reads through the resource manager (which refreshes/touches the LRU entry per read), and forwards Stream/SeekAware reads. `release(handle)` decrements refs and removes the handle at zero. `clear()` resets service state.

Cached data is never stored as a borrowed pointer in an AssetService handle: LRU eviction can invalidate pointers, so cache reads re-enter the manager for each operation. The service remains synchronous and not thread-safe. Retail IDs are intentionally not populated; mappings require verified research.
