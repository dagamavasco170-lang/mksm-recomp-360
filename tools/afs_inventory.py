#!/usr/bin/env python3
"""Read-only, streaming inventory of PS2 AFS archives; never extracts payloads."""
from __future__ import annotations

import argparse
import csv
import hashlib
import json
import math
import struct
from collections import Counter
from pathlib import Path
from typing import Any

ARCHIVES = ("GAMEDATA.WAG", "GAMEDATA.WAH", "GAMEDATA.WAI", "GAMEDATA.WAJ")
CHUNK = 1024 * 1024
PREFIX = 64
SAMPLE = 64 * 1024
SCAN_WINDOW = 4096

# Explicit signatures only. A match embedded in a blob is evidence, not proof
# that the whole AFS entry has that format.
SIGNATURES = (
    (b"\x89PNG\r\n\x1a\n", "image", "PNG signature", "high"),
    (b"\xff\xd8\xff", "image", "JPEG signature", "high"),
    (b"DDS ", "texture", "DDS signature", "high"),
    (b"BM", "image", "BMP signature", "medium"),
    (b"OggS", "audio", "Ogg page signature", "high"),
    (b"VAGp", "audio", "Sony PS2 VAG header", "high"),
    (b"TIM2", "texture", "PS2 TIM2 signature", "high"),
    (b"PK\x03\x04", "archive", "ZIP local-file header", "high"),
    (b"\x1f\x8b", "compressed", "GZIP signature", "high"),
    (b"PWF", "container_candidate", "PWF prefix; format not decoded", "medium"),
    (b"\x7fELF", "executable_candidate", "ELF header", "high"),
    (b"\x00\x00\x01\xba", "mpeg_program_stream", "MPEG program-stream pack start", "high"),
)

def _zlib_header(prefix: bytes) -> bool:
    if len(prefix) < 2:
        return False
    cmf, flg = prefix[0], prefix[1]
    return (cmf & 0x0F) == 8 and (cmf >> 4) <= 7 and ((cmf << 8) | flg) % 31 == 0

def classify(prefix: bytes) -> tuple[str, str, str]:
    if not prefix:
        return "empty", "high", "zero-length AFS entry"
    if prefix.startswith(b"RIFF") and len(prefix) >= 12:
        form = prefix[8:12]
        if form == b"WAVE":
            return "audio", "high", "RIFF/WAVE signature"
        if form == b"WEBP":
            return "image", "high", "RIFF/WEBP signature"
        if form == b"AVI ":
            return "video", "high", "RIFF/AVI signature"
        return "container_candidate", "medium", f"RIFF form {form!r}; subtype unknown"
    for magic, kind, evidence, confidence in SIGNATURES:
        if prefix.startswith(magic):
            return kind, confidence, evidence
    if _zlib_header(prefix):
        return "compressed_stream_candidate", "medium", "valid zlib CMF/FLG header; payload not decompressed"
    return "unknown", "none", "no recognized signature at entry start"

def _entropy(data: bytes) -> float:
    if not data:
        return 0.0
    counts = Counter(data)
    total = len(data)
    return -sum((n / total) * math.log2(n / total) for n in counts.values())

def _embedded_signatures(data: bytes) -> list[dict[str, Any]]:
    found = []
    for magic, kind, evidence, confidence in SIGNATURES:
        start = 1
        while True:
            offset = data.find(magic, start)
            if offset < 0:
                break
            found.append({"offset": offset, "classification": kind,
                          "signature": evidence, "confidence": confidence})
            start = offset + 1
            if len(found) >= 32:
                return found
    return sorted(found, key=lambda item: item["offset"])

def _inspect_entry(f, archive: str, index: int, offset: int, length: int) -> dict[str, Any]:
    f.seek(offset)
    prefix = f.read(min(PREFIX, length))
    kind, confidence, evidence = classify(prefix)

    # A bounded prefix sample provides diagnostic hints only. High entropy is
    # not treated as proof of compression or encryption.
    f.seek(offset)
    sample = f.read(min(SAMPLE, length))
    if sample:
        printable_ratio = sum(32 <= b < 127 or b in (9, 10, 13) for b in sample) / len(sample)
        zero_ratio = sample.count(0) / len(sample)
    else:
        printable_ratio = zero_ratio = 0.0

    digest = hashlib.sha256()
    remaining = length
    f.seek(offset)
    while remaining:
        block = f.read(min(CHUNK, remaining))
        if not block:
            raise ValueError(f"{archive}[{index}]: unexpected EOF while hashing")
        digest.update(block)
        remaining -= len(block)

    return {
        "archive": archive,
        "entry_index": index,
        "offset": offset,
        "size": length,
        "sha256": digest.hexdigest(),
        "prefix_hex": prefix.hex(),
        "classification": kind,
        "confidence": confidence,
        "evidence": evidence,
        "sample_bytes": len(sample),
        "sample_entropy_bits": round(_entropy(sample), 4),
        "sample_printable_ratio": round(printable_ratio, 4),
        "sample_zero_ratio": round(zero_ratio, 4),
        "embedded_signatures": _embedded_signatures(sample[:SCAN_WINDOW]),
        "status": "unclassified" if kind == "unknown" else "classified",
    }

def inventory_archive(path: Path) -> tuple[dict[str, Any], list[dict[str, Any]]]:
    file_size = path.stat().st_size
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
        if table_end > file_size:
            raise ValueError(f"{path.name}: truncated AFS entry table")
        table = f.read(count * 8)
        if len(table) != count * 8:
            raise ValueError(f"{path.name}: incomplete AFS table")

        ranges = []
        rows = []
        for index in range(count):
            offset, length = struct.unpack_from("<II", table, index * 8)
            if offset < table_end or offset > file_size or length > file_size - offset:
                raise ValueError(f"{path.name}[{index}]: invalid range offset={offset} size={length}")
            ranges.append((offset, offset + length, index))
            rows.append(_inspect_entry(f, path.name, index, offset, length))

        # Flag partial overlaps, but permit exact range aliases (shared payload).
        for start, end, index in ranges:
            overlaps = []
            for other_start, other_end, other_index in ranges:
                if index != other_index and start < other_end and other_start < end:
                    if (start, end) != (other_start, other_end):
                        overlaps.append(other_index)
            rows[index]["partial_overlap_indices"] = sorted(overlaps)

    meta = {
        "archive": path.name,
        "file_size": file_size,
        "entry_count": count,
        "table_offset": 8,
        "table_entry_size": 8,
        "table_end": table_end,
        "data_bytes": file_size - table_end,
        "entries_with_partial_overlaps": sum(bool(r["partial_overlap_indices"]) for r in rows),
    }
    return meta, rows

def run(input_dir: Path, output_dir: Path) -> list[dict[str, Any]]:
    rows: list[dict[str, Any]] = []
    archives = []
    missing = []
    for name in ARCHIVES:
        path = input_dir / name
        if path.exists():
            meta, archive_rows = inventory_archive(path)
            archives.append(meta)
            rows.extend(archive_rows)
        else:
            missing.append(name)

    if not archives:
        raise FileNotFoundError(
            f"No supported archive found in {input_dir}; expected one or more of {', '.join(ARCHIVES)}"
        )

    digest_groups: dict[str, list[str]] = {}
    for row in rows:
        digest_groups.setdefault(row["sha256"], []).append(
            f'{row["archive"]}:{row["entry_index"]}'
        )
    for row in rows:
        locations = digest_groups[row["sha256"]]
        row["identical_payload_locations"] = locations if len(locations) > 1 else []

    output_dir.mkdir(parents=True, exist_ok=True)
    fields = [
        "archive", "entry_index", "offset", "size", "sha256", "prefix_hex",
        "classification", "confidence", "evidence", "sample_bytes",
        "sample_entropy_bits", "sample_printable_ratio", "sample_zero_ratio",
        "embedded_signatures", "partial_overlap_indices",
        "identical_payload_locations", "status",
    ]
    csv_rows = []
    for row in rows:
        csv_row = dict(row)
        for key in ("embedded_signatures", "partial_overlap_indices", "identical_payload_locations"):
            csv_row[key] = json.dumps(csv_row[key], separators=(",", ":"))
        csv_rows.append(csv_row)
    with (output_dir / "afs_inventory.csv").open("w", newline="", encoding="utf-8") as f:
        writer = csv.DictWriter(f, fieldnames=fields)
        writer.writeheader()
        writer.writerows(csv_rows)

    document = {
        "schema_version": 2,
        "archives_missing": missing,
        "archives": archives,
        "classification_policy": "Only validated leading signatures receive format labels; embedded signatures and entropy are diagnostic hints.",
        "entries": rows,
    }
    (output_dir / "afs_inventory.json").write_text(
        json.dumps(document, indent=2) + "\n", encoding="utf-8"
    )

    counts = Counter(row["classification"] for row in rows)
    unknown = sum(row["status"] == "unclassified" for row in rows)
    partial_overlaps = sum(bool(row["partial_overlap_indices"]) for row in rows)
    duplicate_entries = sum(bool(row["identical_payload_locations"]) for row in rows)
    report = [
        "AFS resource inventory",
        "=====================",
        f"Archives scanned: {len(archives)}",
        f"Entries scanned: {len(rows)}",
        f"Unclassified entries: {unknown}",
        f"Entries with partial overlaps: {partial_overlaps}",
        f"Entries with identical payload elsewhere: {duplicate_entries}",
        f"Missing archives: {', '.join(missing) if missing else 'none'}",
        "",
        "Archive structure:",
    ]
    report.extend(
        f'  {a["archive"]}: {a["file_size"]} bytes, {a["entry_count"]} entries, table_end={a["table_end"]}'
        for a in archives
    )
    report += ["", "Classification counts:"]
    report.extend(f"  {key}: {value}" for key, value in sorted(counts.items()))
    report += [
        "",
        "Only signature-backed leading formats are classified.",
        "Embedded signatures, entropy, printable ratio and zero ratio are evidence hints only.",
        "SHA-256 is computed incrementally; payloads are never extracted or copied.",
    ]
    (output_dir / "afs_inventory_report.txt").write_text("\n".join(report) + "\n", encoding="utf-8")
    return rows

def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--input-dir", required=True, type=Path,
                        help="folder containing extracted GAMEDATA.WAG/WAH/WAI/WAJ files")
    parser.add_argument("--output-dir", default=Path("afs_inventory"), type=Path)
    args = parser.parse_args()
    rows = run(args.input_dir, args.output_dir)
    print(f"Inventoried {len(rows)} entries. Output: {args.output_dir.resolve()}")
    return 0

if __name__ == "__main__":
    raise SystemExit(main())
