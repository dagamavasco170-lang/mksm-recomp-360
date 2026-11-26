# AFS Runtime Binding Candidates

Tool: `tools/afs_runtime_binding.py`

Joins three evidence streams using archive + entry index:
- candidate classifier output (family/type suggestion, hash, offsets/sizes)
- resource profiler output (trace-based access observations)
- resource intelligence output (cache/stream/seek-aware/on-demand policy recommendation)

Run:
```powershell
py -3 tools\\afs_runtime_binding.py --candidates-json afs_candidates\\afs_candidates.json --profile-json afs_resource_profile\\afs_resource_profile.json --intelligence-json afs_resource_intelligence\\afs_resource_intelligence.json --output-dir afs_runtime_bindings
```

Outputs JSON, CSV, and text report. The type is a heuristic candidate; strategy is trace-informed only when both profile and intelligence data exist. Missing policy defaults to OnDemand. Logical IDs are always null and status is always candidate_only. This output is **not** automatically imported into `AfsResourceCatalog`; a human must validate actual content and runtime behavior first.
