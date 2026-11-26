#!/usr/bin/env python3
"""Join AFS candidate, profiler, and loading-intelligence evidence into conservative runtime bindings.

Bindings are recommendations only. This tool never emits verified resource IDs.
"""
from __future__ import annotations
import argparse, csv, json
from pathlib import Path

TYPE_MAP = {
    "texture_candidate": "Texture", "image_candidate": "Texture",
    "audio_candidate": "Audio", "video_candidate": "Other",
    "nested_archive_candidate": "Other", "compressed_candidate": "Other",
    "container_candidate": "Other", "executable_candidate": "Other",
    "embedded_signature_hint": "Unknown", "unresolved": "Unknown",
}
STRATEGY_MAP = {"CACHE":"Cache", "STREAM":"Stream", "SEEK_AWARE":"SeekAware", "ON_DEMAND":"OnDemand"}

def key(row):
    return (str(row.get("archive","")).upper(), int(row.get("entry_index",-1)))

def index(rows):
    return {key(r):r for r in rows}

def bind(candidates, profile=None, intelligence=None):
    prof=index((profile or {}).get("entries",[]))
    intel=index((intelligence or {}).get("entries",[]))
    result=[]
    for c in candidates.get("entries",[]):
        k=key(c); p=prof.get(k,{}); policy=intel.get(k,{})
        family=c.get("family_candidate","unresolved")
        strategy=policy.get("load_strategy","ON_DEMAND")
        evidence=[]
        if c.get("evidence"): evidence.append({"source":"candidate_classifier","detail":c["evidence"]})
        if p: evidence.append({"source":"resource_profiler","detail":"trace-derived access statistics"})
        if policy: evidence.append({"source":"resource_intelligence","detail":policy.get("strategy_reason","policy recommendation")})
        if c.get("confidence")=="high" and family not in ("unresolved","embedded_signature_hint"):
            type_conf="heuristic_high"
        elif family not in ("unresolved","embedded_signature_hint"):
            type_conf="heuristic"
        else: type_conf="unknown"
        if not policy: strategy="ON_DEMAND"
        result.append({
            "archive":k[0],"entry_index":k[1],"offset":c.get("offset"),"size_bytes":c.get("size"),
            "sha256":c.get("sha256"),"resource_type_suggestion":TYPE_MAP.get(family,"Unknown"),
            "type_confidence":type_conf,"load_strategy_suggestion":STRATEGY_MAP.get(strategy,"OnDemand"),
            "strategy_confidence":"trace_heuristic" if p and policy else "default_safety",
            "status":"candidate_only","logical_resource_id":None,"evidence":evidence,
            "binding_note":"Requires manual validation; candidate records are not retail resource IDs."
        })
    result.sort(key=lambda r:(r["archive"],r["entry_index"]))
    return {"schema_version":1,"status":"candidate_bindings_only","entries":result,
      "summary":{"entries":len(result),"type_suggestions":sum(r["resource_type_suggestion"]!="Unknown" for r in result),
      "trace_policy_suggestions":sum(r["strategy_confidence"]=="trace_heuristic" for r in result),
      "verified_ids":0},"policy_note":"Never auto-promote candidates into AfsResourceCatalog. Confirm entry identity, content, and runtime behavior first."}

def run(candidates_path,outdir,profile_path=None,intelligence_path=None):
    candidates=json.loads(candidates_path.read_text(encoding="utf-8"))
    profile=json.loads(profile_path.read_text(encoding="utf-8")) if profile_path else None
    intelligence=json.loads(intelligence_path.read_text(encoding="utf-8")) if intelligence_path else None
    data=bind(candidates,profile,intelligence); outdir.mkdir(parents=True,exist_ok=True)
    (outdir/"afs_runtime_bindings.json").write_text(json.dumps(data,indent=2)+"\\n",encoding="utf-8")
    rows=data["entries"]; columns=list(rows[0]) if rows else ["archive","entry_index","resource_type_suggestion","load_strategy_suggestion","status"]
    with (outdir/"afs_runtime_bindings.csv").open("w",newline="",encoding="utf-8") as f:
        w=csv.DictWriter(f,fieldnames=columns);w.writeheader();w.writerows(rows)
    lines=["AFS Runtime Binding Candidates","=============================",*[f"{k}: {v}" for k,v in data["summary"].items()],"",
      "Bindings (candidate only):"]
    lines += [f'{r["archive"]}:{r["entry_index"]} -> {r["resource_type_suggestion"]}/{r["load_strategy_suggestion"]} ({r["type_confidence"]}, {r["strategy_confidence"]})' for r in rows]
    lines += ["","No logical IDs assigned. Manual validation is required before runtime catalog activation."]
    (outdir/"afs_runtime_bindings_report.txt").write_text("\\n".join(lines)+"\\n",encoding="utf-8")
    return data

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument("--candidates-json",required=True,type=Path);p.add_argument("--profile-json",type=Path)
    p.add_argument("--intelligence-json",type=Path);p.add_argument("--output-dir",type=Path,default=Path("afs_runtime_bindings"))
    a=p.parse_args();d=run(a.candidates_json,a.output_dir,a.profile_json,a.intelligence_json)
    print(f"Generated {d['summary']['entries']} candidate bindings; verified IDs: 0")
if __name__=="__main__":main()
