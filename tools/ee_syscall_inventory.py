#!/usr/bin/env python3
"""Inventory generated PS2 recompilation syscall/FileIO call sites.

This is an audit tool only: syscall IDs are reported as observed and never
assigned to IOMAN commands based on guesses.
"""
from __future__ import annotations

import argparse
import json
import re
from pathlib import Path

SYSCALL_RE = re.compile(
    r"handleSyscall\s*\(\s*rdram\s*,\s*ctx\s*,\s*"
    r"(0[xX][0-9a-fA-F]+|[0-9]+)\s*[uU]?\s*\)",
    re.MULTILINE,
)
FIO_RE = re.compile(r"\b(?:ps2_syscalls::)?(fioOpen|fioRead|fioLseek|fioClose|fioWrite)\s*\(")
SYSCALL_SITE_RE = re.compile(r"EE_SYSCALL_SITE:\s*pc=(0[xX][0-9a-fA-F]+)\s+id=(0[xX][0-9a-fA-F]+)")


def scan_source(path: Path) -> list[dict]:
    text = path.read_text(encoding="utf-8", errors="replace")
    records: list[dict] = []
    for match in SYSCALL_RE.finditer(text):
        value = int(match.group(1), 0)
        line = text.count("\n", 0, match.start()) + 1
        records.append({
            "source": str(path),
            "line": line,
            "kind": "generated_syscall",
            "syscall_id": f"0x{value:X}",
            "target": None,
        })
    for match in SYSCALL_SITE_RE.finditer(text):
        guest_pc = int(match.group(1), 0)
        syscall_id = int(match.group(2), 0)
        line = text.count("\\n", 0, match.start()) + 1
        records.append({
            "source": str(path),
            "line": line,
            "kind": "generated_syscall_site",
            "guest_pc": f"0x{guest_pc:X}",
            "syscall_id": f"0x{syscall_id:X}",
            "target": None,
        })
    for match in FIO_RE.finditer(text):
        line = text.count("\\n", 0, match.start()) + 1
        records.append({
            "source": str(path),
            "line": line,
            "kind": "direct_fileio_call",
            "syscall_id": None,
            "target": match.group(1),
        })
    return records


def build_report(paths: list[Path]) -> dict:
    records = []
    for path in paths:
        records.extend(scan_source(path))
    records.sort(key=lambda item: (item["source"], item["line"], item["kind"]))
    ids = sorted({r["syscall_id"] for r in records if r["syscall_id"]})
    targets = sorted({r["target"] for r in records if r["target"]})
    return {
        "schema": "mksm-ee-syscall-inventory-v1",
        "source_files": [str(p) for p in paths],
        "syscall_ids_observed": ids,
        "fileio_targets_observed": targets,
        "calls": records,
        "mapping_policy": "No syscall ID is assigned to IOMAN commands without verified call-site evidence.",
    }


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("sources", nargs="+", type=Path, help="generated C/C++ source files")
    parser.add_argument("--json", action="store_true", help="emit JSON report")
    args = parser.parse_args()
    report = build_report(args.sources)
    if args.json:
        print(json.dumps(report, indent=2))
    else:
        print("Observed syscall IDs:", ", ".join(report["syscall_ids_observed"]) or "(none)")
        print("Observed direct FileIO targets:", ", ".join(report["fileio_targets_observed"]) or "(none)")
        for call in report["calls"]:
            detail = call["syscall_id"] or call["target"]
            print(f'{call["source"]}:{call["line"]}: {call["kind"]} {detail}')
        print(report["mapping_policy"])
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
