# AFS Catalog Resource Manager

`ps2_resource_afs_catalog_manager.h` adds logical ID resolution across WAG/WAH/WAI/WAJ using the manifest `AfsResourceCatalog`.

- `resolve(id)`: resolves logical ID only if the catalog validates (no duplicate IDs or archive-entry pairs).
- `loadCached(id)`: only Cache strategy.
- `loadOnDemand(id,out)`: only OnDemand strategy.
- `read(id,offset,out)`: Stream and SeekAware strategies, bounded buffer.
- `findByEntry(archive,index)`: reverse lookup for trace/catalog correlation.

The runtime uses the record's archive and entry. No guessed retail mapping was added; the verified manifest remains empty until metadata evidence exists. The test uses synthetic AFS fixtures and exercises all four archives/policies.
