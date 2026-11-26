import struct
import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from afs_inventory import classify, inventory_archive, run

def make_afs(entries):
    count = len(entries)
    offset = 8 + count * 8
    table = bytearray()
    payload = bytearray()
    for data in entries:
        table += struct.pack("<II", offset, len(data))
        payload += data
        offset += len(data)
    return b"AFS\0" + struct.pack("<I", count) + table + payload

class AfsInventoryTests(unittest.TestCase):
    def test_signature_classification_and_unknown(self):
        self.assertEqual(classify(b"\x89PNG\r\n\x1a\n")[0], "image")
        self.assertEqual(classify(b"VAGp")[0], "audio")
        self.assertEqual(classify(b"RIFFxxxxWAVE")[0], "audio")
        self.assertEqual(classify(b"\x00\x01opaque")[0], "unknown")
        self.assertEqual(classify(b"")[0], "empty")

    def test_zlib_header_is_candidate_only(self):
        self.assertEqual(classify(b"\x78\x9c" + b"x" * 10)[0], "compressed_stream_candidate")

    def test_inventory_hash_and_offsets(self):
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / "GAMEDATA.WAG"
            path.write_bytes(make_afs([b"\x89PNG\r\n\x1a\nDATA", b"opaque"]))
            meta, rows = inventory_archive(path)
            self.assertEqual(meta["entry_count"], 2)
            self.assertEqual(len(rows), 2)
            self.assertEqual(rows[0]["entry_index"], 0)
            self.assertEqual(rows[0]["classification"], "image")
            self.assertEqual(rows[1]["status"], "unclassified")
            self.assertEqual(rows[0]["size"], 12)
            self.assertEqual(len(rows[0]["sha256"]), 64)
            self.assertIn("sample_entropy_bits", rows[0])

    def test_rejects_truncated_table_and_invalid_range(self):
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / "bad.afs"
            path.write_bytes(b"AFS\0" + struct.pack("<I", 1) + b"\0" * 3)
            with self.assertRaises(ValueError):
                inventory_archive(path)
            path.write_bytes(b"AFS\0" + struct.pack("<I", 1) + struct.pack("<II", 16, 99))
            with self.assertRaises(ValueError):
                inventory_archive(path)

    def test_outputs_metadata_only(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            source = root / "input"
            output = root / "out"
            source.mkdir()
            (source / "GAMEDATA.WAG").write_bytes(make_afs([b"VAGp" + b"x" * 8]))
            rows = run(source, output)
            self.assertEqual(len(rows), 1)
            self.assertTrue((output / "afs_inventory.csv").exists())
            self.assertTrue((output / "afs_inventory.json").exists())
            self.assertTrue((output / "afs_inventory_report.txt").exists())

    def test_fails_when_no_archive_is_found(self):
        with tempfile.TemporaryDirectory() as tmp:
            with self.assertRaises(FileNotFoundError):
                run(Path(tmp), Path(tmp) / "out")

if __name__ == "__main__":
    unittest.main()
