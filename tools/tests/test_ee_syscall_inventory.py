import tempfile
import unittest
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

from ee_syscall_inventory import build_report


class EeSyscallInventoryTests(unittest.TestCase):
    def test_extracts_numeric_generated_syscalls_and_fileio_calls(self):
        source = """void generated() {
 // EE_SYSCALL_SITE: pc=0x1000 id=0x123
 runtime->handleSyscall(rdram, ctx, 0x123u);
 runtime->handleSyscall(
   rdram, ctx, 45U);
 ps2_syscalls::fioOpen(rdram, ctx, runtime);
 fioRead(rdram, ctx, runtime);
}
"""
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / "sample.cpp"
            path.write_text(source, encoding="utf-8")
            report = build_report([path])
        self.assertEqual(report["syscall_ids_observed"], ["0x2D", "0x123"])
        self.assertEqual(report["fileio_targets_observed"], ["fioOpen", "fioRead"])
        self.assertEqual(len(report["calls"]), 5)
        site = next(c for c in report["calls"] if c["kind"] == "generated_syscall_site")
        self.assertEqual(site["guest_pc"], "0x1000")
        self.assertEqual(site["syscall_id"], "0x123")
        self.assertEqual(report["schema"], "mksm-ee-syscall-inventory-v1")
        self.assertIn("No syscall ID is assigned", report["mapping_policy"])

    def test_template_placeholder_is_not_mistaken_for_id(self):
        source = "runtime->handleSyscall(rdram, ctx, 0x{:X}u);"
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / "template.cpp"
            path.write_text(source, encoding="utf-8")
            report = build_report([path])
        self.assertEqual(report["syscall_ids_observed"], [])


if __name__ == "__main__":
    unittest.main()
