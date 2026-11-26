#!/usr/bin/env python3
"""Run IOMAN log extraction, AFS correlation, and resource profiling in one pass."""
from __future__ import annotations
import argparse
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

def build_commands(log: Path, inventory: Path, output: Path, candidates: Path | None = None):
    trace_dir = output / "trace"
    correlation_dir = output / "correlation"
    profile_dir = output / "profile"
    trace_json = trace_dir / "events.json"
    correlation_json = correlation_dir / "ioman_afs_correlation.json"
    commands = [
        [sys.executable, str(ROOT / "tools/ioman_trace_log_extract.py"),
         "--log", str(log), "--output", str(trace_json)],
        [sys.executable, str(ROOT / "tools/ioman_afs_correlate.py"),
         "--trace", str(trace_json), "--inventory-json", str(inventory),
         "--output-dir", str(correlation_dir)],
    ]
    if candidates is not None:
        commands[-1].extend(["--candidates-json", str(candidates)])
    commands.append([
        sys.executable, str(ROOT / "tools/afs_resource_profiler.py"),
        "--correlation-json", str(correlation_json), "--output-dir", str(profile_dir),
    ])
    return commands

def run_pipeline(log: Path, inventory: Path, output: Path, candidates: Path | None = None):
    for required in (log, inventory):
        if not required.is_file():
            raise FileNotFoundError(required)
    if candidates is not None and not candidates.is_file():
        raise FileNotFoundError(candidates)
    output.mkdir(parents=True, exist_ok=True)
    commands = build_commands(log, inventory, output, candidates)
    for command in commands:
        subprocess.run(command, cwd=ROOT, check=True)
    return output

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--log", required=True, type=Path, help="Xbox/Xenia log")
    parser.add_argument("--inventory-json", required=True, type=Path, help="AFS inventory JSON")
    parser.add_argument("--candidates-json", type=Path, help="Optional AFS candidate report JSON")
    parser.add_argument("--output-dir", type=Path, default=Path("ioman_afs_analysis"))
    args = parser.parse_args()
    output = run_pipeline(args.log, args.inventory_json, args.output_dir, args.candidates_json)
    print(f"Pipeline complete. Outputs: {output.resolve()}")

if __name__ == "__main__":
    main()
