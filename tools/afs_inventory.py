#!/usr/bin/env python3
"""Read-only, streaming inventory of PS2 AFS archives; never extracts payloads."""
from __future__ import annotations
import argparse, csv, hashlib, json, struct
from pathlib import Path

ARCHIVES = ("GAMEDATA.WAG", "GAMEDATA.WAH", "GAMEDATA.WAI", "GAMEDATA.WAJ")
CHUNK = 1024 * 1024
PREFIX = 64

def classify(prefix: bytes) -> tuple[str, str, str]:
    # Only high-confidence, explicit file/container signatures are named.
    signatures = (
        (b"\x89PNG\r\n\x1a\n", "image", "PNG signature"),
        (b"\xff\xd8\xff", "image", "JPEG signature"),
        (b"DDS ", "texture", "DDS signature"),
        (b"BM", "image", "BMP signature"),
        (b"RIFF", "audio_or_media", "RIFF container; subtype requires RIFF form inspection"),
        (b"OggS", "audio", "Ogg page signature"),
        (b"VAGp", "audio", "Sony PS2 VAG header"),
        (b"TIM2", "texture", "PS2 TIM2 signature"),
        (b"PK\x03\x04", "archive", "ZIP local-file header"),
        (b"\x1f\x8b", "compressed", "GZIP signature"),
        (b"PWF", "container_candidate", "PWF prefix; format not decoded"),
    )
    for magic, kind, evidence in signatures:
        if prefix.startswith(magic):
            return kind, "high", evidence
    # zlib streams have variable headers; deliberately avoid guessing.
    return "unknown", "none", "no recognized signature in first 64 bytes"

def inventory_archive(path: Path) -> list[dict]:
    size = path.stat().st_size
    with path.open("rb") as f:
        header = f.read(8)
        if len(header) != 8:
            raise ValueError(f"{path.name}: truncated AFS header")
        if header[:4] != b"AFS\0":
            raise ValueError(f"{path.name}: bad AFS magic {header[:4]!r}")
        count = struct.unpack_from("<I", header, 4)[0]
        if count > 1_000_000:
            raise ValueError(f"{path.name}: unreasonable entry count {count}")
        table_end = 8 + count * 8
        if table_end > size:
            raise ValueError(f"{path.name}: truncated AFS entry table")
        table = f.read(count * 8)
        if len(table) != count * 8:
            raise ValueError(f"{path.name}: incomplete AFS table")
        rows = []
        for index in range(count):
            offset, length = struct.unpack_from("<II", table, index * 8)
            if offset < table_end or offset > size or length > size - offset:
                raise ValueError(f"{path.name}[{index}]: invalid range offset={offset} size={length}")
            f.seek(offset)
            prefix = f.read(min(PREFIX, length))
            kind, confidence, evidence = classify(prefix)
            digest = hashlib.sha256()
            remaining = length
            f.seek(offset)
            while remaining:
                block = f.read(min(CHUNK, remaining))
                if not block:
                    raise ValueError(f"{path.name}[{index}]: unexpected EOF while hashing")
                digest.update(block)
                remaining -= len(block)
            rows.append({
                "archive": path.name, "entry_index": index, "offset": offset,
                "size": length, "sha256": digest.hexdigest(),
                "prefix_hex": prefix.hex(), "classification": kind,
                "confidence": confidence, "evidence": evidence,
                "status": "classified" if kind != "unknown" else "unclassified",
            })
        return rows

def run(input_dir: Path, output_dir: Path) -> list[dict]:
    rows = []
    missing = []
    for name in ARCHIVES:
        path = input_dir / name
        if path.exists():
            rows.extend(inventory_archive(path))
        else:
            missing.append(name)
    output_dir.mkdir(parents=True, exist_ok=True)
    fields = ["archive","entry_index","offset","size","sha256","prefix_hex",
              "classification","confidence","evidence","status"]
    with (output_dir / "afs_inventory.csv").open("w", newline="", encoding="utf-8") as f:
        writer = csv.DictWriter(f, fieldnames=fields); writer.writeheader(); writer.writerows(rows)
    (output_dir / "afs_inventory.json").write_text(
        json.dumps({"schema_version":1,"archives_missing":missing,"entries":rows}, indent=2)+"\n",
        encoding="utf-8")
    counts = {}
    for row in rows:
        counts[row["classification"]] = counts.get(row["classification"], 0) + 1
    report = ["AFS resource inventory", "=====================", f"Entries scanned: {len(rows)}",
              f"Missing archives: {', '.join(missing) if missing else 'none'}", "",
              "Classification counts:"]
    report.extend(f"  {key}: {value}" for key,value in sorted(counts.items()))
    report += ["", "Only signature-backed labels are assigned. Unknown entries remain unclassified.",
               "SHA-256 is computed incrementally; payloads are never extracted or copied."]
    (output_dir / "afs_inventory_report.txt").write_text("\n".join(report)+"\n", encoding="utf-8")
    return rows

def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--input-dir", required=True, type=Path, help="folder containing extracted GAMEDATA.WA* files")
    parser.add_argument("--output-dir", default=Path("afs_inventory"), type=Path)
    args = parser.parse_args()
    rows = run(args.input_dir, args.output_dir)
    print(f"Inventoried {len(rows)} entries. Output: {args.output_dir.resolve()}")
    return 0

if __name__ == "__main__":
    raise SystemExit(main())
