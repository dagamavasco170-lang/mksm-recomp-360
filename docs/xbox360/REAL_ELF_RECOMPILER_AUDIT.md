# Real ELF Recompiler Audit

The real-ELF intake pipeline emits a conservative function inventory. PS2Recomp
now writes a structured JSON report alongside generated output:

`ps2_recomp_report.json` (schema version 1) contains counters, per-function
status records, and diagnostic events. The audit tool accepts this JSON or
legacy textual logs and correlates structured function/event data with the ELF
inventory by guest start address. Legacy text logs are correlated by exact
function name.

Run:

```powershell
py -3 tools/audit_ps2recomp_report.py build/real-elf/generated/ps2_recomp_report.json --functions build/real-elf/function_inventory.csv --out-dir build/real-elf/audit
```

Run regression tests:

```powershell
py -3 -m unittest tools.tests.test_audit_ps2recomp_report -v
```

Disposition values are intentionally conservative. A decoded function means
instruction decoding completed; it does not prove generated C++ correctness,
EE semantic correctness, or target execution. No status emitted by this audit
tool means runtime-ready. Functions outside the ELF triage candidate set remain
blocked pending manual review.
