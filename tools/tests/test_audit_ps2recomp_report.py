#!/usr/bin/env python3
"""Regression tests for structured and legacy PS2Recomp audit correlation."""
import csv
import json
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

SCRIPT = Path(__file__).resolve().parents[1] / "audit_ps2recomp_report.py"


class AuditReportTests(unittest.TestCase):
    def run_audit(self, report, inventory):
        temp = tempfile.TemporaryDirectory()
        self.addCleanup(temp.cleanup)
        root = Path(temp.name)
        report_path = root / "report.json"
        inventory_path = root / "inventory.csv"
        out_dir = root / "out"
        report_path.write_text(json.dumps(report) if not isinstance(report, str) else report,
                               encoding="utf-8")
        with inventory_path.open("w", newline="", encoding="utf-8") as stream:
            writer = csv.DictWriter(stream, fieldnames=["address", "size", "name", "triage"])
            writer.writeheader()
            writer.writerows(inventory)
        proc = subprocess.run(
            [sys.executable, str(SCRIPT), str(report_path), "--functions",
             str(inventory_path), "--out-dir", str(out_dir)],
            capture_output=True, text=True, check=False)
        self.assertEqual(proc.returncode, 0, proc.stdout + proc.stderr)
        with (out_dir / "function_audit.csv").open(newline="", encoding="utf-8") as stream:
            rows = list(csv.DictReader(stream))
        summary = json.loads((out_dir / "recompiler_audit.json").read_text(encoding="utf-8"))
        return rows, summary

    def test_structured_events_correlate_by_guest_address(self):
        report = {
            "schema_version": 1, "counters": {},
            "functions": [{"name": "renamed_by_recompiler", "start": 4096,
                           "status": "decoded"}],
            "events": [{"severity": "error", "category": "decode",
                        "function": "different_symbol_name", "address": 4096,
                        "message": "bad instruction"}],
        }
        rows, summary = self.run_audit(report, [
            {"address": "0x1000", "size": "16", "name": "elf_symbol", "triage": "candidate"}
        ])
        self.assertEqual(rows[0]["disposition"], "translation-error-review")
        self.assertEqual(rows[0]["error_events"], "1")
        self.assertEqual(summary["unbound_event_count"], 0)

    def test_decoded_is_not_runtime_ready(self):
        report = {"schema_version": 1, "counters": {},
                  "functions": [{"start": 8192, "status": "decoded"}], "events": []}
        rows, _ = self.run_audit(report, [
            {"address": "0x2000", "size": "4", "name": "f", "triage": "candidate"}
        ])
        self.assertEqual(rows[0]["disposition"], "decoded-awaiting-codegen-validation")

    def test_legacy_text_report_still_correlates_by_name(self):
        rows, summary = self.run_audit("========== PS2Recomp report ==========\n", [
            {"address": "0x3000", "size": "4", "name": "legacy_fn", "triage": "candidate"}
        ])
        self.assertEqual(rows[0]["disposition"], "no-reported-event-unverified")
        self.assertEqual(summary["report_format"], "legacy-text")


if __name__ == "__main__":
    unittest.main()
