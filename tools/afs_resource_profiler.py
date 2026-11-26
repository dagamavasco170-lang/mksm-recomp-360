#!/usr/bin/env python3
"""Profile metadata-only IOMAN/AFS correlations; rankings are heuristics, not asset identification."""
from __future__ import annotations
import argparse, csv, json, math
from pathlib import Path

def profile(correlation):
    rows=[]
    for src in correlation.get("entries", []):
        r=dict(src)
        size=max(0,int(r.get("size") or 0))
        opens=max(0,int(r.get("opens") or 0)); reads=max(0,int(r.get("reads") or 0))
        bytes_read=max(0,int(r.get("read_bytes") or 0)); seeks=max(0,int(r.get("seeks") or 0))
        failures=max(0,int(r.get("failed_operations") or 0)); events=max(0,int(r.get("trace_event_count") or 0))
        r["size_bytes"]=size
        r["read_coverage_ratio"]=round(bytes_read/size,4) if size else None
        r["bytes_per_open"]=round(bytes_read/opens,2) if opens else 0
        r["reads_per_open"]=round(reads/opens,2) if opens else 0
        r["seek_to_read_ratio"]=round(seeks/reads,4) if reads else (float(seeks) if seeks else 0)
        r["failure_rate"]=round(failures/events,4) if events else 0
        r["access_score"]=round(math.log1p(bytes_read)*2 + reads*1.5 + opens*2 + seeks*0.75,3)
        # Metadata-only suggestions: do not infer true resource semantics from access counts.
        r["cache_candidate"]=bool(size and size<=262144 and opens>=2 and reads>=2)
        r["streaming_candidate"]=bool(size>=1048576 and bytes_read>0 and bytes_read<size*0.75)
        r["seek_activity_candidate"]=bool(seeks>=3 and seeks>=reads)
        r["low_usage_candidate"]=bool(size and size<=65536 and opens<=1 and reads<=2 and bytes_read<=size)
        rows.append(r)
    rows.sort(key=lambda x:(-x["access_score"],x["archive"],x["entry_index"]))
    summary={"correlated_entries":len(rows),"total_open_count":sum(int(r.get("opens",0) or 0) for r in rows),
      "total_read_count":sum(int(r.get("reads",0) or 0) for r in rows),
      "total_bytes_read":sum(int(r.get("read_bytes",0) or 0) for r in rows),
      "cache_candidates":sum(r["cache_candidate"] for r in rows),
      "streaming_candidates":sum(r["streaming_candidate"] for r in rows),
      "seek_activity_candidates":sum(r["seek_activity_candidate"] for r in rows),
      "low_usage_candidates":sum(r["low_usage_candidate"] for r in rows)}
    return {"schema_version":1,"summary":summary,"entries":rows,
      "methodology":"access_score = 2*ln(1+read_bytes) + 1.5*reads + 2*opens + 0.75*seeks; candidate flags are heuristics."}

def run(input_path, output_dir):
    data=json.loads(input_path.read_text(encoding="utf-8"))
    result=profile(data); output_dir.mkdir(parents=True,exist_ok=True)
    (output_dir/"afs_resource_profile.json").write_text(json.dumps(result,indent=2)+"\n",encoding="utf-8")
    rows=result["entries"]
    columns=list(rows[0].keys()) if rows else ["archive","entry_index","access_score","size_bytes","opens","reads","read_bytes","seeks","read_coverage_ratio","bytes_per_open","reads_per_open","seek_to_read_ratio","failure_rate","cache_candidate","streaming_candidate","seek_activity_candidate","low_usage_candidate"]
    with (output_dir/"afs_resource_profile.csv").open("w",newline="",encoding="utf-8") as f:
        writer=csv.DictWriter(f,fieldnames=columns); writer.writeheader(); writer.writerows(rows)
    lines=["AFS Resource Access Profile","==========================","",*([f"{k}: {v}" for k,v in result["summary"].items()]),"",
      "Top entries by access score:"]
    lines += [f'  {r["archive"]}:{r["entry_index"]} score={r["access_score"]} bytes={r.get("read_bytes",0)} reads={r.get("reads",0)} opens={r.get("opens",0)} seeks={r.get("seeks",0)} flags='+",".join(k for k in ("cache_candidate","streaming_candidate","seek_activity_candidate","low_usage_candidate") if r[k]) for r in rows[:30]]
    lines += ["","Candidate flags are heuristic and must be validated with runtime evidence."]
    (output_dir/"afs_resource_profile_report.txt").write_text("\n".join(lines)+"\n",encoding="utf-8")
    return result

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument("--correlation-json",required=True,type=Path)
    p.add_argument("--output-dir",type=Path,default=Path("afs_resource_profile"))
    args=p.parse_args()
    result=run(args.correlation_json,args.output_dir)
    print(f"Profiled {result['summary']['correlated_entries']} correlated entries; output: {args.output_dir}")
if __name__=="__main__": main()
