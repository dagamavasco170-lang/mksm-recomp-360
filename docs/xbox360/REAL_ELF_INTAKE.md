# Real PS2 EE ELF intake

This path accepts a **user-supplied local ELF** without adding the ELF, disc image,
AFS archives, or extracted assets to Git.

## 1. Preflight and local config generation

From the repository root:

```powershell
py -3 tools/prepare_real_ps2_elf.py "D:\\PS2\\SLUS_XXXXX.ELF" --out-dir build/real-elf
```

The script validates the ELF identity (ELF32, little-endian, EM_MIPS), header/table
bounds, PT_LOAD file and virtual ranges, segment sizes/alignment, overlapping
virtual mappings, and whether the entry point is in an executable PT_LOAD. The
entry-point check can be relaxed only with the explicit
`--allow-entry-outside-exec` option after manual review.

It writes locally:
- `build/real-elf/elf_validation.json`: validated header, load map, symbol count,
  entry-point result and primary-opcode histogram;
- `build/real-elf/function_inventory.csv`: named STT_FUNC symbols from ELF
  SYMTAB/DYNSYM (may be empty for stripped files);
- `build/real-elf/recompile.real-elf.toml`: absolute-path configuration for the
  existing `ps2_recomp` executable, output directed to
  `build/real-elf/generated`.

Then invoke the existing recompiler:

```powershell
cmake --build build --config Release --target ps2_recomp
.`build\\ps2xRecomp\\Release\\ps2_recomp.exe` build/real-elf/recompile.real-elf.toml
```

The executable path depends on the generator (for Ninja/Make it is commonly
`build/ps2xRecomp/ps2_recomp`). The TOML is intentionally conservative: it does
not apply game-specific syscall/COP0/cache patches or infer Ghidra function maps.

## 2. Interpreting the report

The opcode histogram counts 32-bit words in executable PT_LOAD file bytes and is
only a **primary-opcode inventory**. It is not a decoder-support matrix: embedded
data can be counted as instructions, and R5900 SPECIAL/MMI/FPU sub-opcodes require
instruction-level decoding. The recompiler's own report remains authoritative for
functions translated, stubs, skipped functions, decode failures and unhandled
instructions.

## 3. Safety and limitations

- Keep the ELF and generated output local; do not commit proprietary game binaries
  or assets. Share only reports/logs after checking that they contain no secrets.
- A successful preflight proves structural checks only. It does not prove complete
  PS2 EE compatibility, successful XEX generation, Xenia execution, or physical
  Xbox 360 boot.
- Stripped ELF files may not contain function symbols; use the project's Ghidra
  export workflow to supply function boundaries when available.
- Relocation semantics, overlapping physical aliases, EE-specific load rules and
  code/data classification need further title-specific auditing.
