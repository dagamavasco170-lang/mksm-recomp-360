import json
import tempfile
import unittest
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from ee_syscall_mapping_audit import audit, load_mapping


class EeSyscallMappingAuditTests(unittest.TestCase):
    def test_observed_ids_are_unresolved_without_evidence(self):
        result = audit({"syscall_ids_observed": ["0x12", "0x3", "0x12"]})
        self.assertEqual(result["observed_ids"], ["0x3", "0x12"])
        self.assertEqual([x["syscall_id"] for x in result["unresolved_ids"]], ["0x3", "0x12"])
        self.assertEqual(result["verified_mappings"], [])

    def test_applies_only_explicit_mapping_and_reports_unused(self):
        mapping_data = {
            "schema": "mksm-ee-syscall-mapping-v1",
            "mappings": [
                {"syscall_id": "18", "command": "Read", "evidence": "handler table entry + call-site review"},
                {"syscall_id": "0x99", "command": "Open", "evidence": "confirmed symbol cross-reference"},
            ],
        }
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / "map.json"
            path.write_text(json.dumps(mapping_data), encoding="utf-8")
            mapping = load_mapping(path)
        result = audit({"syscall_ids_observed": ["0x12"]}, mapping)
        self.assertEqual(result["verified_mappings"][0]["command"], "Read")
        self.assertEqual(result["unresolved_ids"], [])
        self.assertEqual(result["mapping_entries_not_observed"], ["0x99"])

    def test_rejects_mapping_without_evidence_or_unknown_command(self):
        cases = [
            {"schema": "mksm-ee-syscall-mapping-v1", "mappings": [{"syscall_id": "0x1", "command": "Open", "evidence": ""}]},
            {"schema": "mksm-ee-syscall-mapping-v1", "mappings": [{"syscall_id": "0x1", "command": "Delete", "evidence": "evidence"}]},
        ]
        for data in cases:
            with self.subTest(data=data), tempfile.TemporaryDirectory() as tmp:
                path = Path(tmp) / "map.json"
                path.write_text(json.dumps(data), encoding="utf-8")
                with self.assertRaises(ValueError):
                    load_mapping(path)


if __name__ == "__main__":
    unittest.main()
