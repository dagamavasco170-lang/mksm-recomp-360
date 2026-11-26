import tempfile
import unittest
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from ee_dispatch_table_extract import extract_dispatch_table, parse_case_value


class EeDispatchTableExtractTests(unittest.TestCase):
    def test_normalizes_signed_and_unsigned_case_labels(self):
        self.assertEqual(parse_case_value("0x2D"), 45)
        self.assertEqual(parse_case_value("static_cast<uint32_t>(-0x1A)"), 0xFFFFFFE6)

    def test_extracts_grouped_cases(self):
        source = """switch (syscallNumber) {
case 0x18:
case 0xFC:
    SetAlarm(rdram, ctx, runtime);
    return true;
case static_cast<uint32_t>(-0x1A):
    iEnableIntc(rdram, ctx, runtime);
    return true;
}
"""
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / "Dispatcher.cpp"
            path.write_text(source, encoding="utf-8")
            report = extract_dispatch_table(path)
        self.assertEqual(report["unique_syscall_count"], 3)
        pairs = {(entry["syscall_id"], entry["handler"]) for entry in report["entries"]}
        self.assertIn(("0x00000018", "SetAlarm"), pairs)
        self.assertIn(("0x000000FC", "SetAlarm"), pairs)
        self.assertIn(("0xFFFFFFE6", "iEnableIntc"), pairs)

    def test_empty_file_has_no_entries(self):
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / "empty.cpp"
            path.write_text("void empty() {}", encoding="utf-8")
            report = extract_dispatch_table(path)
        self.assertEqual(report["entries"], [])


if __name__ == "__main__":
    unittest.main()
