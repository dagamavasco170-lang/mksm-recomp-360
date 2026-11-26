# One-command IOMAN → AFS analysis

This wrapper runs the existing metadata-only pipeline in order: log extraction,
AFS inventory correlation, then access profiling. It does not inspect or copy
file payloads and it does not infer resource IDs.

## Windows (PowerShell)

```powershell
py -3 tools\ioman_afs_pipeline.py --log xenia.log --inventory-json afs_inventory\afs_inventory.json --candidates-json afs_candidates\afs_candidates.json --output-dir ioman_afs_analysis
```

The candidate report is optional; omit `--candidates-json` if unavailable.

## Outputs

- `trace/events.json`: extracted IOMAN metadata events.
- `correlation/ioman_afs_correlation.json`, `.csv`, and report: entries accessed and read/seek/open/close counts.
- `profile/afs_resource_profile.json`, `.csv`, and report: heuristic access profile.

This pipeline currently analyzes the diagnostic smoke bridge trace. The repository
has not yet connected the trace ring to a complete gameplay runtime, so this
workflow must not be described as a real gameplay trace until that integration
exists and the log contains AFS entry paths from gameplay.
