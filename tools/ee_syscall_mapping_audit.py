#!/usr/bin/env python3
"""Audit generated EE syscall IDs against an explicit evidence-backed mapping.

Mapping JSON format:
{
  "schema": "mksm-ee-syscall-mapping-v1",
  "mappings": [
    {"syscall_id": "0x123", "command": "Open", "evidence": "verified handler/call-site reference"}
  ]
}
Only evidence-backed mappings are accepted. This tool does not guess commands.
"""
from __future__ import annotations

import argparse
import json
from pathlib import Path

COMMANDS = {"Open", "Read", "Write", "Lseek", "Close"}


def normalize_id(value: str) -> str:
    return f"0x{int(value, 0):X}"


def load_mapping(path: Path) -> dict[str, dict]:
    data = json.loads(path.read_text(encoding="utf-8"))
    if data.get("schema") != "mksm-ee-syscall-mapping-v1":
        raise ValueError("mapping schema must be mksm-ee-syscall-mapping-v1")
    result = {}
    for item in data.get("mappings", []):
        syscall_id = normalize_id(str(item.get("syscall_id", "")))
        command = item.get("command")
        evidence = item.get("evidence")
        if command not in COMMANDS:
            raise ValueError(f"{syscall_id}: unsupported command {command!r}")
        if not isinstance(evidence, str) or not evidence.strip():
            raise ValueError(f"{syscall_id}: evidence is required")
        if syscall_id in result:
            raise ValueError(f"duplicate mapping for {syscall_id}")
        result[syscall_id] = {"command": command, "evidence": evidence.strip()}
    return result


def audit(inventory: dict, mapping: dict[str, dict] | None = None) -> dict:
    observed = sorted({normalize_id(x) for x in inventory.get("syscall_ids_observed", [])})
    mapping = mapping or {}
    mapped = []
    unresolved = []
    for syscall_id in observed:
        item = mapping.get(syscall_id)
        if item:
            mapped.append({"syscall_id": syscall_id, **item})
        else:
            unresolved.append({"syscall_id": syscall_id, "status": "unresolved"})
    unused = sorted(set(mapping) - set(observed))
    return {
        "schema": "mksm-ee-syscall-mapping-audit-v1",
        "observed_ids": observed,
        "verified_mappings": mapped,
        "unresolved_ids": unresolved,
        "mapping_entries_not_observed": unused,
        "policy": "Mappings require explicit evidence; this audit never infers syscall semantics.",
    }


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("inventory", type=Path, help="JSON emitted by ee_syscall_inventory.py")
    parser.add_argument("--mapping", type=Path, help="optional evidence-backed mapping JSON")
    parser.add_argument("--json", action="store_true", help="emit JSON report")
    args = parser.parse_args()
    inventory = json.loads(args.inventory.read_text(encoding="utf-8"))
    mapping = load_mapping(args.mapping) if args.mapping else {}
    report = audit(inventory, mapping)
    if args.json:
        print(json.dumps(report, indent=2))
    else:
        print("Verified mappings:")
        for item in report["verified_mappings"]:
            print(f'  {item["syscall_id"]} -> {item["command"]} [{item["evidence"]}]')
        print("Unresolved IDs:", ", ".join(x["syscall_id"] for x in report["unresolved_ids"]) or "(none)")
        print("Mapping entries not observed:", ", ".join(report["mapping_entries_not_observed"]) or "(none)")
        print(report["policy"])
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
