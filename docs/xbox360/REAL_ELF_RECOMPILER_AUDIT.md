# Real ELF → PS2Recomp diagnostic correlation

After running `tools/prepare_real_ps2_elf.py`, run the recompiler with its
locally generated TOML and capture the complete text report:

```powershell
.\build\path\to\ps2_recomp.exe build\real-elf\recompile.real-elf.toml *> build\real-elf\ps2_recomp.log
```

The executable path depends on the local CMake generator/build configuration.
Then correlate the report with the inventory:

```powershell
py -3 tools/audit_ps2recomp_report.py build\real-elf\ps2_recomp.log --functions build\real-elf\function_inventory.csv --out-dir build\real-elf\audit
```

Outputs:

- `recompiler_audit.json`: counters, all parsed events, severity/category counts,
  and events that could not be tied to an ELF symbol.
- `function_audit.csv`: ELF triage plus warning/error events grouped by exact
  function symbol name.

Disposition meanings:

- `elf-triage-blocked`: the ELF preflight did not classify the symbol as a
  complete, aligned, file-backed executable candidate.
- `translation-error-review`: one or more error events were correlated.
- `warning-review`: warnings were correlated and no errors were found.
- `no-reported-event-unverified`: no event was correlated. This is explicitly
  **not** evidence of successful translation.

The current text reporter does not expose a complete per-function success
manifest, so the tool preserves unmatched events and does not infer a
function's success from silence. A later stage must add structured reporting
at the recompiler's function-processing boundary and separately validate
generated blocks on the host/Xenia and physical Xbox 360.
