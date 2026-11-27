# Real ELF → PS2Recomp structured audit

The recompiler writes `ps2_recomp_report.json` directly into its configured
output directory after successful code generation. The report contains:

- Aggregate counters from the existing reporter.
- Per-function guest start/end addresses, symbol names, processing status, and reason.
- Structured diagnostic events with severity, category, guest address, function, and message.

Run the ELF preflight first:

```powershell
py -3 tools/prepare_real_ps2_elf.py "D:\\path\\to\\real.elf" --out-dir build/real-elf
```

Run ps2_recomp with the generated TOML. Its configured output directory
(default from this intake flow: `build/real-elf/generated`) will contain
`ps2_recomp_report.json`.

Then correlate by guest function start address:

```powershell
py -3 tools/audit_ps2recomp_report.py build/real-elf/generated/ps2_recomp_report.json --functions build/real-elf/function_inventory.csv --out-dir build/real-elf/audit
```

The audit writes `recompiler_audit.json` and `function_audit.csv`. It also
continues to accept a legacy text log for backwards compatibility. Structured
function and event records are correlated by guest start address; legacy text
events are correlated by exact function name.

Status interpretation:

- `decoded-awaiting-codegen-validation`: instruction decoding completed.
  This is not proof that generated C++ compiles or is semantically correct.
- `decoded-entrypoint-awaiting-validation`: discovered entrypoint decoded,
  but still requires generated-code and execution validation.
- `decode-failed`: no usable decode result was produced for the function.
- `stubbed-not-guest-code`: a configured/runtime stub will be emitted.
- `skipped-not-generated-guest-code`: configured skip path.
- `translation-error-review` / `warning-review`: correlated diagnostics require review.
- `elf-triage-blocked`: the ELF inventory did not classify the symbol as a
  complete aligned executable candidate.

No status means runtime-ready. Generated-code compilation, host/Xenia execution,
EE semantic validation, and physical Xbox 360 testing remain independent gates.

## Regression tests

Run the structured-address and legacy-log correlation tests:

```powershell
py -3 -m unittest tools.tests.test_audit_ps2recomp_report -v
```
