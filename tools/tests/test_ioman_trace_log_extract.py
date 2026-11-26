import sys, unittest
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
from ioman_trace_log_extract import parse_lines, run

class IomanLogExtractTests(unittest.TestCase):
    def test_parses_event_lines_and_ignores_other_logs(self):
        log="""noise line
MKSM_IOMAN_EVENT op=open path=afs0:/GAMEDATA.WAG/4 fd=21 a1=1 a2=0 result=21
[debug] MKSM_IOMAN_EVENT op=read path= fd=21 a1=8192 a2=64 result=64
MKSM_IOMAN_EVENT op=lseek path= fd=21 a1=0 a2=0 result=0
"""
        events=parse_lines(log)
        self.assertEqual(len(events),3)
        self.assertEqual(events[0]["path"],"afs0:/GAMEDATA.WAG/4")
        self.assertEqual(events[1]["operation"],"read")
        self.assertEqual(events[1]["arg2"],64)
        self.assertEqual(events[2]["result"],0)
    def test_malformed_marker_fails(self):
        with self.assertRaises(ValueError):
            parse_lines("MKSM_IOMAN_EVENT not-a-valid-record")
    def test_writes_correlator_json(self):
        import json,tempfile
        with tempfile.TemporaryDirectory() as d:
            root=Path(d); log=root/"xenia.log"; out=root/"events.json"
            log.write_text("MKSM_IOMAN_EVENT op=close path= fd=8 a1=0 a2=0 result=0")
            events=run(log,out)
            self.assertEqual(events[0]["descriptor"],8)
            self.assertEqual(json.loads(out.read_text())["events"][0]["operation"],"close")
if __name__=="__main__": unittest.main()
