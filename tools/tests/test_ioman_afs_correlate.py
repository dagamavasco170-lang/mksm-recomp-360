import json, sys, tempfile, unittest
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
from ioman_afs_correlate import correlate, parse_afs_path, run

class CorrelatorTests(unittest.TestCase):
    def setUp(self):
        self.inventory={"entries":[{"archive":"GAMEDATA.WAG","entry_index":4,"offset":200,
            "size":12,"sha256":"d"*64,"classification":"audio","confidence":"high"}]}
        self.candidates={"entries":[{"archive":"GAMEDATA.WAG","entry_index":4,
            "family_candidate":"audio_candidate"}]}
    def test_parse_paths(self):
        self.assertEqual(parse_afs_path("afs0:/GAMEDATA.WAG/4"),("GAMEDATA.WAG",4))
        self.assertEqual(parse_afs_path("afs0:/GAMEDATA.WAH/12"),("GAMEDATA.WAH",12))
        self.assertIsNone(parse_afs_path("cdrom0:/file"))
    def test_aggregates_and_matches_inventory(self):
        events=[{"operation":"open","path":"afs0:/GAMEDATA.WAG/4","result":100},
            {"operation":"read","descriptor":100,"arg2":8,"result":8},
            {"operation":"read","descriptor":100,"arg2":4,"result":4},
            {"operation":"lseek","descriptor":100,"result":0},
            {"operation":"close","descriptor":100,"result":0},
            {"operation":"open","path":"afs0:/GAMEDATA.WAH/2","result":101}]
        r=correlate(events,self.inventory,self.candidates)
        self.assertEqual(r["summary"]["afs_entries_touched"],2)
        row=r["entries"][0]
        self.assertEqual(row["entry_index"],4)
        self.assertEqual(row["candidate_family"],"audio_candidate")
        self.assertEqual(row["read_bytes"],12)
        self.assertEqual(row["opens"],1)
        self.assertFalse(r["entries"][1]["inventory_found"])
    def test_enum_command_values(self):
        events=[{"command":1,"path":"afs0:/GAMEDATA.WAG/4","result":7},
            {"command":3,"descriptor":7,"result":3},{"command":2,"descriptor":7,"result":0}]
        r=correlate(events,self.inventory)
        self.assertEqual(r["entries"][0]["opens"],1)
        self.assertEqual(r["entries"][0]["reads"],1)
        self.assertEqual(r["entries"][0]["closes"],1)
    def test_json_trace_run_outputs(self):
        with tempfile.TemporaryDirectory() as d:
            root=Path(d); trace=root/"trace.json"; inv=root/"inventory.json"; cand=root/"candidates.json"
            trace.write_text(json.dumps({"events":[{"op":"open","path":"afs0:/GAMEDATA.WAG/4","result":5}]}))
            inv.write_text(json.dumps(self.inventory)); cand.write_text(json.dumps(self.candidates))
            result=run(trace,inv,root/"out",cand)
            self.assertEqual(result["summary"]["inventory_matches"],1)
            self.assertTrue((root/"out"/"ioman_afs_correlation.csv").exists())
            self.assertTrue((root/"out"/"ioman_afs_correlation_report.txt").exists())
if __name__=="__main__": unittest.main()
