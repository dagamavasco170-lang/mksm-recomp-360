#!/usr/bin/env python3
"""Build conservative AFS resource-family candidates from inventory metadata."""
from __future__ import annotations
import argparse, csv, json
from collections import defaultdict
from pathlib import Path

FAMILY_MAP = {
    "texture": "texture_candidate",
    "image": "image_candidate",
    "audio": "audio_candidate",
    "video": "video_candidate",
    "mpeg_program_stream": "video_candidate",
    "archive": "nested_archive_candidate",
    "compressed": "compressed_candidate",
    "compressed_stream_candidate": "compressed_candidate",
    "container_candidate": "container_candidate",
    "executable_candidate": "executable_candidate",
}

def candidate(row):
    classification = row.get("classification", "unknown")
    family = FAMILY_MAP.get(classification, "unresolved")
    confidence = row.get("confidence", "none")
    evidence = row.get("evidence", "")
    notes = []
    if family == "unresolved" and row.get("embedded_signatures"):
        families = sorted({FAMILY_MAP.get(x.get("classification", ""), "unresolved")
                           for x in row["embedded_signatures"]})
        families = [x for x in families if x != "unresolved"]
        if families:
            family = "embedded_signature_hint"
            notes.append("Embedded signatures may be nested content; not proof of entry type.")
            evidence = "; ".join(families)
    if family == "unresolved":
        notes.append("No recognized primary signature; requires parser or runtime trace.")
    if row.get("sample_entropy_bits", 0) >= 7.5:
        notes.append("High sample entropy: diagnostic only, not proof of compression/encryption.")
    if row.get("sample_printable_ratio", 0) >= 0.8:
        notes.append("Sample is mostly printable bytes; text/string inspection candidate.")
    if row.get("identical_payload_locations"):
        notes.append("Payload hash shared across entries: inspect aliases/reuse.")
    if row.get("partial_overlap_indices"):
        notes.append("AFS ranges partially overlap: validate archive interpretation.")
    return {
        "archive": row["archive"], "entry_index": row["entry_index"],
        "offset": row["offset"], "size": row["size"], "sha256": row["sha256"],
        "family_candidate": family, "confidence": confidence,
        "evidence": evidence, "notes": " ".join(notes),
        "status": "candidate_only"
    }

def run(source: Path, outdir: Path):
    data=json.loads(source.read_text(encoding="utf-8"))
    rows=[candidate(r) for r in data.get("entries", [])]
    groups=defaultdict(list)
    for r in rows:
        groups[r["family_candidate"]].append(r)
    outdir.mkdir(parents=True,exist_ok=True)
    fields=list(rows[0].keys()) if rows else [
        "archive","entry_index","offset","size","sha256","family_candidate",
        "confidence","evidence","notes","status"]
    with (outdir/"afs_candidates.csv").open("w",newline="",encoding="utf-8") as f:
        w=csv.DictWriter(f,fieldnames=fields);w.writeheader();w.writerows(rows)
    (outdir/"afs_candidates.json").write_text(json.dumps({
        "schema_version":1,"input_inventory":str(source),"policy":"Heuristic candidates only; never directly becomes the verified resource catalog.",
        "candidate_groups":{k:[{"archive":r["archive"],"entry_index":r["entry_index"],
            "size":r["size"],"sha256":r["sha256"]} for r in v] for k,v in sorted(groups.items())},
        "entries":rows},indent=2)+"\n",encoding="utf-8")
    report=["AFS candidate families","======================",f"Entries: {len(rows)}","",
            "Counts by candidate family:"]
    report += [f"  {k}: {len(v)}" for k,v in sorted(groups.items())]
    report += ["","Candidate labels are not verified resource types.",
               "Do not populate runtime IDs from this report without confirming evidence."]
    (outdir/"afs_candidates_report.txt").write_text("\n".join(report)+"\n",encoding="utf-8")
    return rows

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument("--inventory-json",required=True,type=Path)
    p.add_argument("--output-dir",default=Path("afs_candidates"),type=Path)
    a=p.parse_args(); rows=run(a.inventory_json,a.output_dir)
    print(f"Generated {len(rows)} candidates at {a.output_dir.resolve()}")
if __name__=="__main__":
    main()
