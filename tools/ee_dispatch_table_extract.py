#!/usr/bin/env python3
"""Extract explicit numeric syscall -> handler mappings from Dispatcher.cpp."""
from __future__ import annotations

import argparse
import json
import re
from pathlib import Path

CASE_RE = re.compile(r"^\s*case\s+(.+?)\s*:\s*$")
CALL_RE = re.compile(r"^\s*([A-Za-z_]\w*)\s*\(\s*rdram\s*,\s*ctx\s*,\s*runtime\s*\)\s*;\s*$")


def parse_case_value(expression: str) -> int:
    expr = expression.strip()
    cast_negative = re.fullmatch(r"static_cast\s*<\s*uint32_t\s*>\s*\(\s*(-\s*(?:0[xX][0-9a-fA-F]+|[0-9]+))\s*\)", expr)
    if cast_negative:
        return int(cast_negative.group(1).replace(" ", ""), 0) & 0xFFFFFFFF
    if re.fullmatch(r"(?:0[xX][0-9a-fA-F]+|[0-9]+)", expr):
        return int(expr, 0) & 0xFFFFFFFF
    raise ValueError(f"unsupported case label: {expression}")


def extract_dispatch_table(source: Path) -> dict:
    lines = source.read_text(encoding="utf-8", errors="replace").splitlines()
    entries = []
    pending = []
    in_dispatch_switch = False
    for index, line in enumerate(lines):
        if "switch (syscallNumber)" in line:
            in_dispatch_switch = True
            pending = []
            continue
        if not in_dispatch_switch:
            continue
        case_match = CASE_RE.match(line)
        if case_match:
            pending.append(case_match.group(1))
            continue
        call_match = CALL_RE.match(line)
        if call_match and pending:
            handler = call_match.group(1)
            for expression in pending:
                entries.append({
                    "syscall_id": f"0x{parse_case_value(expression):08X}",
                    "handler": handler,
                    "source": str(source),
                    "line": index + 1,
                    "case_expression": expression,
                    "evidence": "explicit numeric case in dispatchNumericSyscall",
                })
            pending = []
            continue
        if line.strip() == "}" and in_dispatch_switch:
            in_dispatch_switch = False
            pending = []
    entries.sort(key=lambda item: (int(item["syscall_id"], 16), item["handler"]))
    return {
        "schema": "mksm-ee-dispatch-table-v1",
        "source": str(source),
        "entries": entries,
        "unique_syscall_count": len({item["syscall_id"] for item in entries}),
        "mapping_policy": "Entries are extracted from explicit dispatcher case labels; no guest IDs are inferred.",
    }


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("dispatcher", type=Path, help="Dispatcher.cpp")
    parser.add_argument("--json", action="store_true")
    parser.add_argument("--output", type=Path, help="write extracted table as JSON to this path")
    args = parser.parse_args()
    report = extract_dispatch_table(args.dispatcher)
    rendered = json.dumps(report, indent=2) + "\\n"
    if args.output:
        args.output.write_text(rendered, encoding="utf-8")
    if args.json:
        print(rendered, end="")
    else:
        for item in report["entries"]:
            print(f'{item["syscall_id"]} -> {item["handler"]} ({item["source"]}:{item["line"]})')
        print(f'Unique IDs: {report["unique_syscall_count"]}')
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
