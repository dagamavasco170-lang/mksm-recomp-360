#!/usr/bin/env python3
"""Extract MKSM_IOMAN_EVENT metadata lines from Xbox/Xenia logs into correlator JSON."""
from __future__ import annotations
import argparse, json, re
from pathlib import Path

MARKER = "MKSM_IOMAN_EVENT "
LINE = re.compile(r"\bop=(open|close|read|lseek)\s+path=(.*?)\s+fd=(-?\d+)\s+a1=(\d+)\s+a2=(\d+)\s+result=(-?\d+)")

def parse_lines(text: str):
    events=[]
    for line in text.splitlines():
        pos=line.find(MARKER)
        if pos<0:
            continue
        payload=line[pos+len(MARKER):]
        m=LINE.search(payload)
        if not m:
            raise ValueError(f"malformed IOMAN event marker: {payload}")
        op,path,fd,a1,a2,result=m.groups()
        events.append({"operation":op,"path":path,"descriptor":int(fd),
                      "arg1":int(a1),"arg2":int(a2),"result":int(result)})
    return events

def run(log_path: Path, output_path: Path):
    events=parse_lines(log_path.read_text(encoding="utf-8-sig",errors="replace"))
    output_path.parent.mkdir(parents=True,exist_ok=True)
    output_path.write_text(json.dumps({"schema_version":1,"events":events},indent=2)+"\n",encoding="utf-8")
    return events

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument("--log",required=True,type=Path,help="Xbox/Xenia log containing MKSM_IOMAN_EVENT lines")
    p.add_argument("--output",required=True,type=Path,help="output JSON trace")
    a=p.parse_args()
    events=run(a.log,a.output)
    print(f"Extracted {len(events)} metadata IOMAN events -> {a.output}")
if __name__=="__main__": main()
