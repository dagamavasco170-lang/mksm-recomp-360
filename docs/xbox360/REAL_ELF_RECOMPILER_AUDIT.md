# Real ELF → PS2Recomp structured audit

The recompiler now emits ps2_recomp_report.json directly into its configured
output directory after successful code generation. The report contains:

- Aggregate counters from the existing reporter.
- Per-function guest start/end addresses, symbol names, processing status, and reason.
- Structured diagnostic events with severity, category, guest address, function, and message.

Run the ELF preflight first:

    py -3 tools/prepare_real_ps2_elf.py "D:\path\to\real.elf" --out-dir build/real-elf

Run ps2_recomp with the generated TOML. Its configured output directory
(default from this intake flow: build/real-elf/generated) will contain
ps2_recomp_report.json.

Then correlate by guest function start address:

    py -3 tools/audit_ps2recomp_report.py build/real-elf/generated/ps2_recomp_report.json --functions build/real-elf/function_inventory.csv --out-dir build/real-elf/audit

The audit writes recompiler_audit.json and function_audit.csv. It also
continues to accept a legacy text log for backwards compatibility, but the
structured report is authoritative because it joins functions by guest start
address rather than relying on symbol-name text parsing.

Status interpretation:

- decoded-awaiting-codegen-validation: instruction decoding completed.
  This is not proof that generated C++ compiles or is semantically correct.
- decode-failed: no usable decode result was produced for the function.
- stubbed-not-guest-code: a configured/runtime stub will be emitted.
- skipped-not-generated-guest-code: configured skip path.
- translation-error-review / warning-review: correlated diagnostics require review.
- elf-triage-blocked: the ELF inventory did not classify the symbol as a
  complete aligned executable candidate.

No status means runtime-ready. Generated-code compilation, host/Xenia execution,
EE semantic validation, and physical Xbox 360 testing remain independent gates.
