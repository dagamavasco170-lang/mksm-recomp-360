#!/usr/bin/env python3
"""Validate a user-supplied PS2 EE ELF and prepare a local PS2Recomp config.

No game image, ELF, or extracted asset is copied into the repository. Reports and
TOML are written beside the requested output directory (or to explicit paths).
"""
from __future__ import annotations

import argparse
import csv
import json
import struct
import sys
from collections import Counter
from pathlib import Path

ELF_HEADER = struct.Struct("<16sHHIIIIIHHHHHH")
PROGRAM_HEADER = struct.Struct("<IIIIIIII")
SECTION_HEADER = struct.Struct("<IIIIIIIIII")
SYMBOL = struct.Struct("<IIIBBH")
PT_LOAD, PF_X = 1, 1
SHT_SYMTAB, SHT_DYNSYM, STT_FUNC = 2, 11, 2
EM_MIPS = 8


def fail(message: str) -> None:
    raise ValueError(message)


def c_string(blob: bytes, offset: int) -> str:
    if offset >= len(blob):
        return ""
    end = blob.find(b"\0", offset)
    if end < 0:
        end = len(blob)
    return blob[offset:end].decode("utf-8", errors="replace")


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("elf", type=Path, help="local PS2 EE ELF32 little-endian file")
    ap.add_argument("--out-dir", type=Path, default=Path("build/real-elf"))
    ap.add_argument("--config", type=Path, help="optional explicit TOML output path")
    ap.add_argument("--report", type=Path, help="optional explicit JSON report path")
    ap.add_argument("--functions", type=Path, help="optional explicit function CSV path")
    ap.add_argument("--allow-entry-outside-exec", action="store_true",
                    help="record, but do not reject, an entry point outside executable PT_LOAD")
    args = ap.parse_args()

    elf_path = args.elf.expanduser().resolve(strict=True)
    data = elf_path.read_bytes()
    if len(data) < ELF_HEADER.size:
        fail("file is shorter than an ELF32 header")
    fields = ELF_HEADER.unpack_from(data)
    ident = fields[0]
    if ident[:4] != b"\x7fELF":
        fail("invalid ELF magic")
    if ident[4] != 1:
        fail(f"expected ELFCLASS32 (1), got EI_CLASS={ident[4]}")
    if ident[5] != 1:
        fail(f"expected little-endian (1), got EI_DATA={ident[5]}")
    if ident[6] != 1:
        fail(f"unsupported ELF identification version {ident[6]}")

    (_, e_type, machine, version, entry, phoff, shoff, flags,
     ehsize, phentsize, phnum, shentsize, shnum, shstrndx) = fields
    if machine != EM_MIPS:
        fail(f"expected EM_MIPS ({EM_MIPS}), got e_machine={machine}")
    if version != 1:
        fail(f"unsupported ELF version {version}")
    if ehsize < ELF_HEADER.size:
        fail(f"invalid ELF header size {ehsize}")
    if phnum and phentsize < PROGRAM_HEADER.size:
        fail(f"program header entry size {phentsize} is too small")
    if shnum and shentsize < SECTION_HEADER.size:
        fail(f"section header entry size {shentsize} is too small")
    if phoff + phnum * phentsize > len(data):
        fail("program header table extends beyond end of file")
    if shoff + shnum * shentsize > len(data):
        fail("section header table extends beyond end of file")

    loads, executable_ranges = [], []
    for i in range(phnum):
        off = phoff + i * phentsize
        p_type, p_offset, vaddr, paddr, filesz, memsz, p_flags, align = PROGRAM_HEADER.unpack_from(data, off)
        if p_type != PT_LOAD:
            continue
        if filesz > memsz:
            fail(f"PT_LOAD[{i}] has p_filesz > p_memsz")
        if p_offset + filesz > len(data):
            fail(f"PT_LOAD[{i}] file range exceeds ELF size")
        if vaddr + memsz > 0x100000000:
            fail(f"PT_LOAD[{i}] virtual range overflows 32-bit address space")
        if align not in (0, 1) and (align & (align - 1)):
            fail(f"PT_LOAD[{i}] alignment is not a power of two")
        item = {"index": i, "offset": p_offset, "vaddr": vaddr, "paddr": paddr,
                "filesz": filesz, "memsz": memsz, "flags": p_flags, "align": align}
        loads.append(item)
        if p_flags & PF_X and memsz:
            executable_ranges.append((vaddr, vaddr + memsz, i))
    if not loads:
        fail("ELF has no PT_LOAD segments")
    ordered = sorted(loads, key=lambda x: x["vaddr"])
    for left, right in zip(ordered, ordered[1:]):
        if left["vaddr"] + left["memsz"] > right["vaddr"]:
            fail(f"overlapping PT_LOAD virtual ranges: segment {left['index']} and {right['index']}")
    entry_exec = any(start <= entry < end for start, end, _ in executable_ranges)
    if not entry_exec and not args.allow_entry_outside_exec:
        fail(f"entry point 0x{entry:08X} is not inside an executable PT_LOAD (use --allow-entry-outside-exec only after auditing)")

    sections = []
    for i in range(shnum):
        off = shoff + i * shentsize
        sections.append(SECTION_HEADER.unpack_from(data, off))
    functions = []
    for idx, sh in enumerate(sections):
        name_off, stype, sflags, addr, offset, size, link, info, align, entsize = sh
        if stype not in (SHT_SYMTAB, SHT_DYNSYM):
            continue
        if offset + size > len(data) or link >= len(sections):
            fail(f"symbol table section {idx} has an invalid range or string-table link")
        strings_sh = sections[link]
        strings_off, strings_size = strings_sh[4], strings_sh[5]
        if strings_off + strings_size > len(data):
            fail(f"symbol table section {idx} has an invalid string table")
        strings = data[strings_off:strings_off + strings_size]
        stride = entsize or SYMBOL.size
        if stride < SYMBOL.size:
            fail(f"symbol table section {idx} entry size is too small")
        for pos in range(offset, offset + size - SYMBOL.size + 1, stride):
            name, value, sym_size, info_byte, other, shndx = SYMBOL.unpack_from(data, pos)
            if (info_byte & 0x0F) != STT_FUNC or value == 0:
                continue
            functions.append({"address": value, "size": sym_size,
                              "name": c_string(strings, name), "section_index": shndx,
                              "source_table": idx})
    functions.sort(key=lambda x: (x["address"], x["name"]))
    unique = []
    seen = set()
    for fn in functions:
        key = (fn["address"], fn["name"])
        if key not in seen:
            seen.add(key)
            unique.append(fn)

    opcode_counts = Counter()
    for seg in loads:
        if not (seg["flags"] & PF_X):
            continue
        start, size = seg["offset"], seg["filesz"]
        usable = size - (size % 4)
        for pos in range(start, start + usable, 4):
            word = struct.unpack_from("<I", data, pos)[0]
            opcode_counts[f"{word >> 26:02X}"] += 1

    out_dir = args.out_dir.expanduser().resolve()
    out_dir.mkdir(parents=True, exist_ok=True)
    config_path = args.config.expanduser().resolve() if args.config else out_dir / "recompile.real-elf.toml"
    report_path = args.report.expanduser().resolve() if args.report else out_dir / "elf_validation.json"
    csv_path = args.functions.expanduser().resolve() if args.functions else out_dir / "function_inventory.csv"
    for path in (config_path, report_path, csv_path):
        path.parent.mkdir(parents=True, exist_ok=True)

    def toml_quote(value: str) -> str:
        return json.dumps(value, ensure_ascii=False)
    generated_dir = (out_dir / "generated").resolve()
    config = ("# Generated locally by tools/prepare_real_ps2_elf.py\n"
              "# Input ELF remains outside the repository. Review game-specific patches separately.\n"
              "[general]\n"
              f"input = {toml_quote(str(elf_path))}\n"
              f"output = {toml_quote(str(generated_dir))}\n"
              "single_file_output = false\n"
              "low_memory_mode = true\n"
              "patch_syscalls = false\n"
              "patch_cop0 = false\n"
              "patch_cache = false\n")
    config_path.write_text(config, encoding="utf-8")

    with csv_path.open("w", newline="", encoding="utf-8") as f:
        writer = csv.DictWriter(f, fieldnames=["address", "size", "name", "section_index", "source_table"])
        writer.writeheader()
        for fn in unique:
            writer.writerow({**fn, "address": f"0x{fn['address']:08X}",
                             "size": fn["size"]})
    report = {
        "schema": 1, "input": str(elf_path), "file_size": len(data),
        "elf": {"class": 32, "endianness": "little", "machine": "EM_MIPS",
                "machine_id": machine, "type": e_type, "flags": f"0x{flags:08X}",
                "entry_point": f"0x{entry:08X}", "entry_in_executable_load": entry_exec},
        "program_headers": {"load_segment_count": len(loads), "segments": loads},
        "sections": {"count": shnum}, "functions": {"symbol_count": len(unique),
                "inventory_csv": str(csv_path)},
        "instruction_words_in_executable_file_bytes": sum(opcode_counts.values()),
        "primary_opcode_histogram": dict(sorted(opcode_counts.items())),
        "coverage_note": "Heuristic primary-opcode histogram only; it is not a decoder support verdict and includes data embedded in executable segments.",
        "recompiler_config": str(config_path),
        "limitations": ["Stripped ELFs may have no function symbols.",
                        "PT_LOAD overlap and basic ELF identity are checked; PS2-specific relocations and EE hardware semantics are not certified.",
                        "Do not treat this preflight as proof that the ELF fully recompiles or boots."]
    }
    report_path.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    print(f"PS2_ELF_PREFLIGHT_PASS machine=EM_MIPS class=32 endian=little entry=0x{entry:08X} loads={len(loads)} functions={len(unique)}")
    print(f"config={config_path}")
    print(f"report={report_path}")
    print(f"functions={csv_path}")
    print("opcode histogram is heuristic; run ps2_recomp with the generated config for translation diagnostics")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (OSError, ValueError, struct.error) as exc:
        print(f"PS2_ELF_PREFLIGHT_FAIL: {exc}", file=sys.stderr)
        raise SystemExit(2)
