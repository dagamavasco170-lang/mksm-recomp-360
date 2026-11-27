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

    def test_legacy_event_correlates_by_name_and_address(self):
        legacy = (
            "========== PS2Recomp report ==========\n"
            "  [warning] decode function=legacy_fn addr=0x3000 - unsupported guest operation\n"
        )
        rows, summary = self.run_audit(legacy, [
            {"address": "0x3000", "size": "4", "name": "legacy_fn", "triage": "candidate"}
        ])
        self.assertEqual(rows[0]["disposition"], "warning-review")
        self.assertEqual(rows[0]["warning_events"], "1")
        self.assertEqual(summary["report_format"], "legacy-text")
        self.assertEqual(summary["unbound_event_count"], 0)

    def test_structured_event_without_address_falls_back_to_exact_name(self):
        report = {
            "schema_version": 1, "counters": {}, "functions": [],
            "events": [{"severity": "warning", "category": "control-flow",
                        "function": "named_fn", "address": None, "message": "manual review"}],
        }
        rows, summary = self.run_audit(report, [
            {"address": "0x4000", "size": "8", "name": "named_fn", "triage": "candidate"}
        ])
        self.assertEqual(rows[0]["disposition"], "warning-review")
        self.assertEqual(rows[0]["warning_events"], "1")
        self.assertEqual(summary["unbound_event_count"], 0)

    def test_unknown_structured_address_is_preserved_as_unbound(self):
        report = {
            "schema_version": 1, "counters": {}, "functions": [],
            "events": [{"severity": "error", "category": "decode",
                        "function": "not_in_inventory", "address": 0xDEADBEEF,
                        "message": "address not found"}],
        }
        rows, summary = self.run_audit(report, [
            {"address": "0x5000", "size": "4", "name": "other_fn", "triage": "candidate"}
        ])
        self.assertEqual(rows[0]["disposition"], "no-reported-event-unverified")
        self.assertEqual(summary["unbound_event_count"], 1)
        self.assertEqual(summary["unbound_events"][0]["address"], 0xDEADBEEF)

    def test_elf_triage_blocked_takes_precedence_over_translation_error(self):
        report = {
            "schema_version": 1, "counters": {}, "functions": [],
            "events": [{"severity": "error", "category": "decode",
                        "function": "bad_symbol", "address": 0x6000,
                        "message": "bad decode"}],
        }
        rows, _ = self.run_audit(report, [
            {"address": "0x6000", "size": "0", "name": "bad_symbol", "triage": "needs-boundary"}
        ])
        self.assertEqual(rows[0]["disposition"], "elf-triage-blocked")
        self.assertEqual(rows[0]["error_events"], "1")

    def test_legacy_empty_report_is_not_treated_as_success(self):
        rows, summary = self.run_audit("========== PS2Recomp report ==========\n", [
            {"address": "0x7000", "size": "4", "name": "unseen_fn", "triage": "candidate"}
        ])
        self.assertEqual(rows[0]["disposition"], "no-reported-event-unverified")
        self.assertEqual(summary["report_format"], "legacy-text")
        self.assertEqual(summary["event_count"], 0)


if __name__ == "__main__":
    unittest.main()
