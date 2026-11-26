import importlib.util
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location("ioman_afs_pipeline", ROOT / "ioman_afs_pipeline.py")
module = importlib.util.module_from_spec(spec)
sys.modules[spec.name] = module
spec.loader.exec_module(module)

def test_build_commands_contains_three_stages_and_paths(tmp_path):
    log = tmp_path / "xenia.log"
    inventory = tmp_path / "inventory.json"
    out = tmp_path / "out"
    candidates = tmp_path / "candidates.json"
    commands = module.build_commands(log, inventory, out, candidates)
    assert len(commands) == 3
    assert "--log" in commands[0] and str(log) in commands[0]
    assert "--trace" in commands[1] and "--candidates-json" in commands[1]
    assert "--correlation-json" in commands[2]
    assert str(out / "profile") in commands[2]

def test_candidates_is_optional(tmp_path):
    commands = module.build_commands(tmp_path / "log", tmp_path / "inv", tmp_path / "out")
    assert len(commands) == 3
    assert "--candidates-json" not in commands[1]
