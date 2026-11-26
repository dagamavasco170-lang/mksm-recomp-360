# AFS Resource Access Profiler

Profiles entries from `ioman_afs_correlation.json` and ranks observed IOMAN access activity. It does not inspect or extract archive payloads.

```powershell
py -3 tools\afs_resource_profiler.py --correlation-json "ioman_afs_correlation\ioman_afs_correlation.json" --output-dir "afs_resource_profile"
```

Outputs:
- `afs_resource_profile.json`: metrics and ranking
- `afs_resource_profile.csv`: spreadsheet-friendly table
- `afs_resource_profile_report.txt`: top activity entries and summary

Derived metrics: read coverage (bytes read / archive-entry size; may exceed 1 for rereads), bytes/opens, reads/opens, seek/read ratio, failure rate, and access score.

Score formula: `2*ln(1+read_bytes) + 1.5*reads + 2*opens + 0.75*seeks`. Ranking is a prioritization aid only.

Candidate heuristics:
- cache: entry <= 256 KiB, at least 2 opens and 2 reads
- streaming: entry >= 1 MiB, some bytes read, less than 75% of size observed
- seek activity: at least 3 seeks and seeks >= reads
- low usage: entry <= 64 KiB, <=1 open, <=2 reads, and bytes <= size

These flags are not confirmed resource types or access patterns. The current trace captures seek counts, not seek destinations or chronology, so sequential/random behavior is not inferred. Validate with a real runtime trace before optimizing.

Next integration: collect a trace through the runtime's optional IOMAN sink, correlate it with the AFS inventory, then run this profiler. Hardware execution is required only when the trace comes from the game running on console.
