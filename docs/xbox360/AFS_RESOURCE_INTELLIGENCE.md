# AFS Resource Intelligence

Consumes `afs_resource_profile.json` and emits evidence-tagged loading/cache recommendations. It does not read archive payloads or implement runtime allocation.

```powershell
py -3 tools\afs_resource_intelligence.py --profile-json "afs_resource_profile\afs_resource_profile.json" --output-dir "afs_resource_intelligence"
```

Defaults: 16 MiB cache budget, 64 MiB reference RAM budget. Both can be overridden with `--cache-budget` and `--ram-budget` (bytes).

Strategies:
- `CACHE`: <=256 KiB, repeatedly opened/read, profiler cache flag, and fits remaining budget.
- `STREAM`: >=1 MiB with profiler partial-read flag.
- `SEEK_AWARE`: seek activity flag; seek targets and sequential/random behavior remain unknown.
- `ON_DEMAND`: fallback, weak evidence, budget exhaustion, or unknown size.

Cache allocation is greedy in profiler score order and cannot exceed cache budget. The RAM budget is only a reference; no runtime memory has been measured. This is a research recommendation, not a shipping policy.

Outputs: JSON, CSV, and text report under the output directory. All decisions must be validated with console runtime traces before they are adopted.
