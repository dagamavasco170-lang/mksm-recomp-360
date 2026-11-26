import tempfile
import unittest
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from ee_dispatch_table_extract import extract_dispatch_table, parse_case_value


class EeDispatchTableExtractTests(unittest.TestCase):
    def test_normalizes_positive_and_signed_cases(self):
        self.assertEqual(parse_case_value("0x2D"), 0x2D)
        self.assertEqual(parse_case_value("static_cast<uint32_t>(-0x1A)"), 0xFFFFFFE6)

    def test_extracts_grouped_cases_and_handler(self):
        source = """bool dispatchNumericSyscall(uint32_t syscallNumber) {
    switch (syscallNumber) {
    case 0x18:
    case 0xFC:
        SetAlarm(rdram, ctx, runtime);
        return true;
    case static_cast<uint32_t>(-0x1A):
        iEnableIntc(rdram, ctx, runtime);
        return true;
    }
    return false;
}
"""
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / "Dispatcher.cpp"
            path.write_text(source, encoding="utf-8")
            report = extract_dispatch_table(path)
        self.assertEqual(report["unique_syscall_count"], 3)
        pairs = {(e["syscall_id"], e["handler"]) for e in report["entries"]}
        self.assertIn(("0x00000018", "SetAlarm"), pairs)
        self.assertIn(("0x000000FC", "SetAlarm"), pairs)
        self.assertIn(("0xFFFFFFE6", "iEnableIntc"), pairs)

    def test_empty_dispatcher(self):
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / "empty.cpp"
            path.write_text("bool x() { return false; }", encoding="utf-8")
            report = extract_dispatch_table(path)
        self.assertEqual(report["entries"], [])


if __name__ == "__main__":
    unittest.main()
