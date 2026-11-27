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

## Host regression checks

The reporter JSON writer has a focused C++ test target. Configure the host
build with PS2Recomp enabled, build the test, then run it through CTest:

```powershell
cmake -S . -B build/host -DPS2X_BUILD_RUNTIME=OFF -DPS2X_BUILD_ANALYZER=OFF -DPS2X_BUILD_TEST=OFF -DPS2X_BUILD_STUDIO=OFF
cmake --build build/host --target ps2recomp_reporter_json_test
ctest --test-dir build/host --output-on-failure -R ps2recomp_reporter_json_write
```

The audit parser regression suite exercises structured-address correlation,
legacy logs, unknown addresses, duplicate symbol names, ELF triage precedence,
and conservative function dispositions:

```powershell
py -3 -m unittest discover -s tools/tests -p "test_audit_ps2recomp_report.py" -v
```

## R5900 translator coverage gate

The host CTest target `r5900_translation_coverage` drives decoded R5900 words
through the production `R5900Decoder` and `CodeGenerator::translateInstruction`.
It checks representative EE integer, quadword-memory, COP0, FPU, MMI, and VU
translations against expected emitted C++ fragments. It also asserts that
unsupported primary opcodes and SPECIAL, REGIMM, MMI, FPU, COP0, and VU decode
paths are surfaced through the unhandled-instruction diagnostic
instead of being emitted as fake zero values or silent no-ops.

Run it with:

```powershell
cmake --build build/host --target r5900_translation_coverage_test
ctest --test-dir build/host --output-on-failure -R r5900_translation_coverage
```

This is an instruction-emission coverage gate, not a proof of cycle accuracy or
full EE semantics. Unsupported instructions remain explicit blockers in the
recompiler report; they must be implemented and tested before the related game
functions can be considered runtime-ready.
## Generated-code semantic execution gate

The native host CI now executes the PS2Recomp-generated synthetic EE function,
not just builds it. This probe checks emitted code against explicit expected
state for integer arithmetic, branch/delay-slot behavior, register values,
loads/stores, byte/halfword access, sign extension, RDRAM wrap, unaligned
LQ/SQ, LWL/LWR/SWL/SWR at all four byte offsets, and the generated MMI
HI/LO lane probe. The probe returns nonzero on any mismatch. The Xenia guest
smoke additionally requires the generated MMI trace marker to report zero
failures, so a missing or failing MMI trace cannot be hidden behind the overall
smoke marker.


This is a focused synthetic semantic gate; it is not a differential run against
hardware or a complete EE instruction suite. Overflow/trap, COP0 exception,
cycle/timing, and physical-console behavior remain uncovered until dedicated
reference vectors and implementations are added.
## Arithmetic and HI/LO semantic execution coverage

The synthetic generated-code arithmetic probe now executes through the host runner
and checks independent expected values for signed/unsigned MULT, DIV and DIVU,
MFHI/MFLO, divisor-zero cases for signed and unsigned division, and the
INT32_MIN / -1 signed division edge. It also checks the block return PC and
delay-slot state. The arithmetic probe's generated adapter is required by the
CI artifact validation.

This remains focused synthetic coverage, not a hardware differential test.
ADD/SUB overflow exceptions, broader divide edge vectors, COP0 exception state,
cycle/timing behavior, and physical-console validation remain open.
