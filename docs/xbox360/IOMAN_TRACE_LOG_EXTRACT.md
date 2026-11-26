# IOMAN log trace extraction

The Xbox smoke bridge emits metadata-only lines of the form:

```
MKSM_IOMAN_EVENT op=open path=afs0:/GAMEDATA.WAG/4 fd=21 a1=1 a2=0 result=21
MKSM_IOMAN_EVENT op=read path= fd=21 a1=8192 a2=64 result=64
```

Extract lines from a log file:
```sh
python3 tools/ioman_trace_log_extract.py --log xenia.log --output ioman_trace/events.json
python3 tools/ioman_afs_correlate.py --trace ioman_trace/events.json --inventory-json afs_inventory/afs_inventory.json --candidates-json afs_candidates/afs_candidates.json --output-dir ioman_afs_correlation
python3 tools/afs_resource_profiler.py --correlation-json ioman_afs_correlation/ioman_afs_correlation.json --output-dir afs_resource_profile
```

The extractor accepts only the structured `MKSM_IOMAN_EVENT` marker. AFS entry indices must be present in the traced Open path. Other paths remain unmatched by the correlator. These logs contain metadata, not file payload.
