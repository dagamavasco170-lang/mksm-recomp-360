# Runtime Asset Resolver

Header: `port/xbox360/ps2_runtime_asset_resolver.h`

Resolves resources from the verified AFS catalog by logical ID or archive-entry. Exposes metadata (type/strategy), Cache and OnDemand loading, bounded reads for Stream/SeekAware via the existing manager, dependency lists and dependency closure.

Statuses: Resolved, UnknownId, UnsupportedStrategy, LoadFailed, InvalidCatalog (reserved for future explicit invalid-catalog reporting).

Dependencies are supplied as a separate manifest; graph validation checks unresolved endpoints, self edges, and duplicate edges. Dependency closure performs visited-node traversal and handles cycles without infinite loops.

No retail resource IDs or dependencies are guessed. Catalog remains empty until verified data is provided. Unit test uses synthetic AFS fixtures and is wired into host CI.
