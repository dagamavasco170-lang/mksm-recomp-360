#!/usr/bin/env python3
"""Correlate a PS2Recomp text report with the real-ELF function inventory.

This is a diagnostic post-processor: it does not infer decoder coverage from
opcode histograms and never promotes a function to runtime-ready status.
"""
from __future__ import annotations

import argparse
import csv
import json
import re
from collections import Counter, defaultdict
from pathlib import Path

COUNTER_PATTERNS = {
    "functions_discovered": r"Functions discovered:\s*(\d+)",
    "functions_processed": r"Functions processed:\s*(\d+)",
    "functions_recompiled": r"recompiled:\s*(\d+)",
    "functions_stubbed": r"stubs:\s*(\d+)",
    "functions_skipped": r"skipped:\s*(\d+)",
    "decode_failures": r"decode failures:\s*(\d+)",
    "additional_entrypoints": r"Additional entrypoints:\s*(\d+)",
    "generated_functions": r"Generated functions:\s*(\d+)",
    "unhandled_instructions": r"Unhandled instructions:\s*(\d+)",
    "critical_guest_fallbacks": r"Correctness-critical guest fallbacks:\s*(\d+)",
    "critical_failures": r"Correctness-critical guest fallbacks:\s*\d+, failures:\s*(\d+)",
    "warnings": r"Warnings:\s*(\d+)",
    "errors": r"errors:\s*(\d+)",
}
EVENT_RE = re.compile(
    r"^\s*\[(info|warning|error)\]\s+([^\s]+)"
    r"(?:\s+function=(.*?))?(?:\s+addr=(0x[0-9a-fA-F]+))?\s+-\s+(.*)$"
)
RAW_RE = re.compile(r"\braw=(0x[0-9a-fA-F]+)")
ADDR_RE = re.compile(r"\b(?:addr|at)\s+(0x[0-9a-fA-F]+)\b", re.I)


def parse_report(text: str):
    counters = {}
    for key, pattern in COUNTER_PATTERNS.items():
        match = re.search(pattern, text, re.I)
        if match:
            counters[key] = int(match.group(1))
    events = []
    for line in text.splitlines():
        match = EVENT_RE.match(line)
        if not match:
            continue
        severity, category, function, address, message = match.groups()
        raw = RAW_RE.search(message)
        events.append({
            "severity": severity,
            "category": category,
            "function": (function or "").strip(),
            "address": int(address, 16) if address else None,
            "raw_instruction": int(raw.group(1), 16) if raw else None,
            "message": message.strip(),
            "source_line": line.strip(),
        })
    return counters, events


def load_inventory(path: Path):
    with path.open(newline="", encoding="utf-8-sig") as stream:
        rows = list(csv.DictReader(stream))
    for row in rows:
        value = row.get("address", "")
        try:
            row["_address"] = int(value, 0)
        except ValueError:
            row["_address"] = None
    return rows


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("report", type=Path, help="stdout/stderr capture from ps2_recomp")
    ap.add_argument("--functions", type=Path, required=True,
                    help="function_inventory.csv from prepare_real_ps2_elf.py")
    ap.add_argument("--out-dir", type=Path, default=Path("build/real-elf/audit"))
    args = ap.parse_args()

    report_path = args.report.expanduser().resolve(strict=True)
    inventory_path = args.functions.expanduser().resolve(strict=True)
    report_text = report_path.read_text(encoding="utf-8", errors="replace")
    counters, events = parse_report(report_text)
    inventory = load_inventory(inventory_path)

    by_name = defaultdict(list)
    for row in inventory:
        if row.get("name"):
            by_name[row["name"]].append(row)

    function_events = defaultdict(list)
    unbound_events = []
    for event in events:
        name = event["function"]
        matches = by_name.get(name, []) if name else []
        if matches:
            for row in matches:
                function_events[row["address"]].append(event)
        else:
            unbound_events.append(event)

    audited = []
    for row in inventory:
        row_events = function_events.get(row["address"], [])
        errors = sum(e["severity"] == "error" for e in row_events)
        warnings = sum(e["severity"] == "warning" for e in row_events)
        categories = sorted({e["category"] for e in row_events})
        if row.get("triage") != "candidate":
            disposition = "elf-triage-blocked"
        elif errors:
            disposition = "translation-error-review"
        elif warnings:
            disposition = "warning-review"
        else:
            disposition = "no-reported-event-unverified"
        audited.append({
            "address": row.get("address", ""),
            "size": row.get("size", ""),
            "name": row.get("name", ""),
            "elf_triage": row.get("triage", ""),
            "disposition": disposition,
            "error_events": errors,
            "warning_events": warnings,
            "event_categories": ";".join(categories),
            "event_messages": " | ".join(e["message"] for e in row_events),
        })

    out_dir = args.out_dir.expanduser().resolve()
    out_dir.mkdir(parents=True, exist_ok=True)
    json_path = out_dir / "recompiler_audit.json"
    csv_path = out_dir / "function_audit.csv"
    summary = {
        "schema": 1,
        "report": str(report_path),
        "inventory": str(inventory_path),
        "counters": counters,
        "event_count": len(events),
        "event_severity_counts": dict(Counter(e["severity"] for e in events)),
        "event_category_counts": dict(Counter(e["category"] for e in events)),
        "inventory_count": len(inventory),
        "function_disposition_counts": dict(Counter(r["disposition"] for r in audited)),
        "unbound_event_count": len(unbound_events),
        "unbound_events": unbound_events,
        "events": events,
        "limitations": [
            "No event for a function is not proof that it translated correctly.",
            "The text report does not expose a complete per-function success table.",
            "Event-to-function correlation uses exact symbol names; unmatched events remain in unbound_events.",
            "No function is marked runtime-ready by this tool; generated-code execution and EE semantic validation remain separate gates."
        ]
    }
    json_path.write_text(json.dumps(summary, indent=2) + "\n", encoding="utf-8")
    fields = ["address", "size", "name", "elf_triage", "disposition",
              "error_events", "warning_events", "event_categories", "event_messages"]
    with csv_path.open("w", newline="", encoding="utf-8") as stream:
        writer = csv.DictWriter(stream, fieldnames=fields)
        writer.writeheader()
        writer.writerows(audited)

    print(f"PS2_RECOMP_AUDIT_PASS inventory={len(inventory)} events={len(events)}")
    print("dispositions=" + json.dumps(summary["function_disposition_counts"], sort_keys=True))
    print(f"json={json_path}")
    print(f"csv={csv_path}")
    print("NOTE: no-event functions remain unverified; this is not a decoder-coverage or boot verdict.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
