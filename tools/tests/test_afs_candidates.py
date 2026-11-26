import json, tempfile, unittest, sys
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from afs_candidates import candidate, run

class CandidateTests(unittest.TestCase):
    def test_known_type_stays_candidate_only(self):
        row={"archive":"GAMEDATA.WAG","entry_index":3,"offset":128,"size":64,
             "sha256":"a"*64,"classification":"texture","confidence":"high",
             "evidence":"TIM2 signature","sample_entropy_bits":5.2,
             "sample_printable_ratio":0.1,"identical_payload_locations":[],"partial_overlap_indices":[]}
        c=candidate(row)
        self.assertEqual(c["family_candidate"],"texture_candidate")
        self.assertEqual(c["status"],"candidate_only")
    def test_unknown_with_embedded_hint(self):
        row={"archive":"WAH","entry_index":1,"offset":0,"size":90,"sha256":"b"*64,
             "classification":"unknown","confidence":"none","evidence":"unknown",
             "sample_entropy_bits":7.9,"sample_printable_ratio":0.0,
             "embedded_signatures":[{"classification":"audio"}],
             "identical_payload_locations":[],"partial_overlap_indices":[]}
        c=candidate(row)
        self.assertEqual(c["family_candidate"],"embedded_signature_hint")
        self.assertIn("diagnostic only",c["notes"])
    def test_outputs_files(self):
        with tempfile.TemporaryDirectory() as tmp:
            root=Path(tmp); src=root/"in.json"; out=root/"out"
            src.write_text(json.dumps({"entries":[{"archive":"WAG","entry_index":0,"offset":0,
                "size":3,"sha256":"c"*64,"classification":"unknown","confidence":"none",
                "evidence":"none","sample_entropy_bits":0,"sample_printable_ratio":0,
                "embedded_signatures":[],"identical_payload_locations":[],"partial_overlap_indices":[]}]}))
            self.assertEqual(len(run(src,out)),1)
            self.assertTrue((out/"afs_candidates.csv").exists())
            self.assertTrue((out/"afs_candidates.json").exists())
            self.assertTrue((out/"afs_candidates_report.txt").exists())
if __name__=="__main__": unittest.main()
