#!/usr/bin/env python3
"""Generate evidence-tagged resource loading/cache/streaming policy from profiler metadata."""
from __future__ import annotations
import argparse,csv,json
from pathlib import Path

def recommend(profile, cache_budget=16*1024*1024, ram_budget=64*1024*1024):
    entries=[]
    remaining=max(0,int(cache_budget))
    for src in profile.get("entries",[]):
        r=dict(src); size=max(0,int(r.get("size_bytes",r.get("size",0)) or 0))
        score=float(r.get("access_score",0)); read_bytes=max(0,int(r.get("read_bytes",0) or 0))
        opens=max(0,int(r.get("opens",0) or 0)); reads=max(0,int(r.get("reads",0) or 0))
        if size==0: strategy="ON_DEMAND"; reason="missing/zero size prevents memory budgeting"
        elif size<=262144 and opens>=2 and reads>=2 and r.get("cache_candidate"):
            if size<=remaining:
                strategy="CACHE"; remaining-=size; reason="small repeatedly opened/read resource; fits cache budget"
            else:
                strategy="ON_DEMAND"; reason="cache heuristic matched but cache budget exhausted"
        elif size>=1048576 and r.get("streaming_candidate"):
            strategy="STREAM"; reason="large resource with partial-read evidence"
        elif r.get("seek_activity_candidate"):
            strategy="SEEK_AWARE"; reason="observed seek activity; use seek-capable access, pattern not yet known"
        else:
            strategy="ON_DEMAND"; reason="insufficient evidence for permanent cache or streaming"
        r["load_strategy"]=strategy; r["strategy_reason"]=reason
        r["cache_allocated_bytes"]=size if strategy=="CACHE" else 0
        r["priority_score"]=round(score,3)
        entries.append(r)
    entries.sort(key=lambda x:(-x["priority_score"],x["archive"],x["entry_index"]))
    cache_bytes=sum(r["cache_allocated_bytes"] for r in entries)
    # Runtime budgets are recommendations, not measured Xbox memory consumption.
    ram_budget=max(0,int(ram_budget))
    summary={"entry_count":len(entries),"cache_budget_bytes":int(cache_budget),"cache_allocated_bytes":cache_bytes,
      "cache_budget_remaining_bytes":max(0,int(cache_budget)-cache_bytes),"ram_budget_bytes":ram_budget,
      "recommended_cache_fraction":round(cache_bytes/ram_budget,4) if ram_budget else 0,
      "cache_entries":sum(r["load_strategy"]=="CACHE" for r in entries),
      "stream_entries":sum(r["load_strategy"]=="STREAM" for r in entries),
      "seek_aware_entries":sum(r["load_strategy"]=="SEEK_AWARE" for r in entries),
      "on_demand_entries":sum(r["load_strategy"]=="ON_DEMAND" for r in entries)}
    return {"schema_version":1,"summary":summary,"entries":entries,
      "policy_note":"Heuristic loading recommendations from observed metadata; not validated runtime behavior or measured memory footprint."}

def run(input_path,output_dir,cache_budget,ram_budget):
    data=json.loads(input_path.read_text(encoding="utf-8"))
    out=recommend(data,cache_budget,ram_budget); output_dir.mkdir(parents=True,exist_ok=True)
    (output_dir/"afs_resource_intelligence.json").write_text(json.dumps(out,indent=2)+"\n",encoding="utf-8")
    rows=out["entries"]
    columns=list(rows[0].keys()) if rows else ["archive","entry_index","load_strategy","strategy_reason","priority_score","cache_allocated_bytes","size_bytes"]
    with (output_dir/"afs_resource_intelligence.csv").open("w",newline="",encoding="utf-8") as f:
        w=csv.DictWriter(f,fieldnames=columns); w.writeheader(); w.writerows(rows)
    lines=["AFS Resource Intelligence","========================","",*[f"{k}: {v}" for k,v in out["summary"].items()],"","Entry loading recommendations:"]
    lines += [f'  {r["archive"]}:{r["entry_index"]} -> {r["load_strategy"]} ({r["size_bytes"]} bytes; {r["strategy_reason"]})' for r in rows]
    lines += ["","All strategies are heuristic and require real runtime profiling before adoption."]
    (output_dir/"afs_resource_intelligence_report.txt").write_text("\n".join(lines)+"\n",encoding="utf-8")
    return out

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument("--profile-json",required=True,type=Path)
    p.add_argument("--output-dir",type=Path,default=Path("afs_resource_intelligence"))
    p.add_argument("--cache-budget",type=int,default=16*1024*1024,help="cache budget in bytes (default 16 MiB)")
    p.add_argument("--ram-budget",type=int,default=64*1024*1024,help="reference RAM budget in bytes (default 64 MiB)")
    a=p.parse_args(); result=run(a.profile_json,a.output_dir,a.cache_budget,a.ram_budget)
    print(f"Generated loading recommendations for {result['summary']['entry_count']} entries; output: {a.output_dir}")
if __name__=="__main__":main()
