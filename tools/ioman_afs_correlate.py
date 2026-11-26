#!/usr/bin/env python3
"""Correlate metadata-only IOMAN trace events with AFS inventory/candidate reports."""
from __future__ import annotations
import argparse, csv, json, re
from collections import defaultdict
from pathlib import Path

AFS_PATH = re.compile(r"afs0:/([^/\\]+)/(\d+)(?:/.*)?$", re.IGNORECASE)

def load_records(path: Path):
    text = path.read_text(encoding="utf-8-sig")
    if path.suffix.lower() == ".json":
        payload=json.loads(text)
        if isinstance(payload, dict):
            payload=payload.get("events", payload.get("records", []))
        if not isinstance(payload, list):
            raise ValueError("trace JSON must be a list or an object with events/records list")
        return payload
    with path.open(newline="", encoding="utf-8-sig") as f:
        return list(csv.DictReader(f))

def normalize_op(record):
    op=record.get("command", record.get("operation", record.get("op", "")))
    if isinstance(op, int):
        return {1:"open",2:"close",3:"read",4:"lseek"}.get(op, str(op))
    op=str(op).strip().lower()
    return {"iomancommand::open":"open","iomancommand::close":"close",
            "iomancommand::read":"read","iomancommand::lseek":"lseek",
            "seek":"lseek"}.get(op,op)

def parse_afs_path(path):
    if not path:
        return None
    normalized=str(path).replace("\\","/")
    match=AFS_PATH.search(normalized)
    if not match:
        return None
    return match.group(1).upper(), int(match.group(2))

def correlate(trace_records, inventory, candidates=None):
    inv={}
    for row in inventory.get("entries", []):
        inv[(row["archive"].upper(), int(row["entry_index"]))]=row
    cand={}
    if candidates:
        for row in candidates.get("entries", []):
            cand[(row["archive"].upper(), int(row["entry_index"]))]=row
    grouped={}
    unmatched=[]
    for event in trace_records:
        op=normalize_op(event)
        path=event.get("path", "")
        parsed=parse_afs_path(path) if op=="open" else None
        if parsed is None:
            if op=="open" and path:
                unmatched.append({"operation":op,"path":path,"result":event.get("result"),
                                "reason":"not an AFS archive-entry path"})
            continue
        archive,index=parsed
        key=(archive,index)
        entry=inv.get(key)
        c=cand.get(key)
        agg=grouped.setdefault(key,{"archive":archive,"entry_index":index,
            "inventory_found":entry is not None,"offset":entry.get("offset") if entry else None,
            "size":entry.get("size") if entry else None,"sha256":entry.get("sha256") if entry else None,
            "inventory_classification":entry.get("classification") if entry else None,
            "candidate_family":c.get("family_candidate") if c else None,
            "opens":0,"reads":0,"read_bytes":0,"seeks":0,"closes":0,
            "failed_operations":0,"trace_event_count":0,"last_result":None})
        agg["trace_event_count"]+=1
        result=event.get("result")
        try: result=int(result)
        except (TypeError,ValueError): result=None
        agg["last_result"]=result
        if result is not None and result<0: agg["failed_operations"]+=1
        if op=="open": agg["opens"]+=1
        elif op=="read":
            agg["reads"]+=1
            if result is not None and result>0: agg["read_bytes"]+=result
        elif op=="lseek": agg["seeks"]+=1
        elif op=="close": agg["closes"]+=1
        else: agg["unknown_operations"] = agg.get("unknown_operations",0)+1
    rows=sorted(grouped.values(),key=lambda x:(x["archive"],x["entry_index"]))
    summary={"trace_events":len(trace_records),"afs_entries_touched":len(rows),
        "inventory_matches":sum(r["inventory_found"] for r in rows),
        "inventory_missing":sum(not r["inventory_found"] for r in rows),
        "total_reads":sum(r["reads"] for r in rows),
        "total_read_bytes":sum(r["read_bytes"] for r in rows),
        "failed_operations":sum(r["failed_operations"] for r in rows)}
    return {"schema_version":1,"summary":summary,"entries":rows,"unmatched_open_paths":unmatched}

def run(trace_path, inventory_path, output_dir, candidates_path=None):
    trace=load_records(trace_path)
    inventory=json.loads(inventory_path.read_text(encoding="utf-8"))
    candidates=json.loads(candidates_path.read_text(encoding="utf-8")) if candidates_path else None
    report=correlate(trace,inventory,candidates)
    output_dir.mkdir(parents=True,exist_ok=True)
    (output_dir/"ioman_afs_correlation.json").write_text(json.dumps(report,indent=2)+"\n",encoding="utf-8")
    rows=report["entries"]
    fields=list(rows[0].keys()) if rows else ["archive","entry_index","inventory_found","offset","size","sha256","inventory_classification","candidate_family","opens","reads","read_bytes","seeks","closes","failed_operations","trace_event_count","last_result"]
    with (output_dir/"ioman_afs_correlation.csv").open("w",newline="",encoding="utf-8") as f:
        w=csv.DictWriter(f,fieldnames=fields); w.writeheader(); w.writerows(rows)
    s=report["summary"]
    lines=["IOMAN / AFS trace correlation","============================="]
    lines += [f"{k}: {v}" for k,v in s.items()]
    lines += ["","Entry activity (archive:index):"]
    lines += [f'  {r["archive"]}:{r["entry_index"]} — opens={r["opens"]}, reads={r["reads"]}, bytes={r["read_bytes"]}, seeks={r["seeks"]}, closes={r["closes"]}, family={r["candidate_family"] or r["inventory_classification"] or "unresolved"}' for r in rows]
    lines += ["","Correlations are based on traced AFS path indices; candidate classifications remain heuristic."]
    (output_dir/"ioman_afs_correlation_report.txt").write_text("\n".join(lines)+"\n",encoding="utf-8")
    return report

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument("--trace",required=True,type=Path,help="metadata trace JSON or CSV")
    p.add_argument("--inventory-json",required=True,type=Path)
    p.add_argument("--candidates-json",type=Path,default=None)
    p.add_argument("--output-dir",type=Path,default=Path("ioman_afs_correlation"))
    a=p.parse_args()
    r=run(a.trace,a.inventory_json,a.output_dir,a.candidates_json)
    print(f"Correlated {r['summary']['afs_entries_touched']} AFS entry keys; output: {a.output_dir}")
if __name__=="__main__":
    main()
