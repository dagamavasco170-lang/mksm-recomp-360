# IOMAN trace → AFS inventory correlator

This metadata-only tool joins AFS archive-entry paths observed by IOMAN with the
local inventory and optional candidate-family JSON reports.

## Input trace

JSON may be a list of records or an object containing `events` or `records`.
CSV is also supported. Each event should contain an operation (`op`,
`operation`, or `command`), a path for Open events, and the numeric result.
Numeric IOMAN command values are `1=Open`, `2=Close`, `3=Read`, `4=Lseek`.
The result should contain the return value (Open descriptor, Read byte count,
Lseek offset, or negative errno). Example metadata trace:

```json
{"events":[
  {"command":1,"path":"afs0:/GAMEDATA.WAG/7","result":32},
  {"command":3,"descriptor":32,"arg2":4096,"result":4096},
  {"command":4,"descriptor":32,"arg1":4096,"result":4096},
  {"command":2,"descriptor":32,"result":0}
]}
```

## Run

```powershell
py -3 tools\ioman_afs_correlate.py --trace "ioman_trace.json" --inventory-json "afs_inventory\afs_inventory.json" --candidates-json "afs_candidates\afs_candidates.json" --output-dir "ioman_afs_correlation"
```

The `--candidates-json` argument is optional.

Outputs:
- `ioman_afs_correlation.csv`: one row per archive-entry observed at Open, with
  access counts and bytes read aggregated from its trace.
- `ioman_afs_correlation.json`: summary plus entry mappings, inventory metadata,
  SHA-256, and optional candidate labels.
- `ioman_afs_correlation_report.txt`: readable aggregate report.

An entry path observed in a trace is direct runtime evidence of access. Candidate
family labels remain heuristic and are never promoted to verified catalog IDs.
Non-AFS Open paths are retained in a list of unmatched paths for review.
