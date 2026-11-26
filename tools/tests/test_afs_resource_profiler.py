import importlib.util
import pathlib
import unittest

ROOT=pathlib.Path(__file__).resolve().parents[1]
spec=importlib.util.spec_from_file_location("afs_resource_profiler",ROOT/"afs_resource_profiler.py")
module=importlib.util.module_from_spec(spec); spec.loader.exec_module(module)

class ProfilerTests(unittest.TestCase):
    def test_metrics_and_heuristic_flags(self):
        data={"entries":[
          {"archive":"WAH","entry_index":5,"size":100000,"opens":3,"reads":8,"read_bytes":50000,"seeks":0,"failed_operations":0,"trace_event_count":11},
          {"archive":"WAG","entry_index":2,"size":3000000,"opens":1,"reads":2,"read_bytes":100000,"seeks":4,"failed_operations":1,"trace_event_count":8},
          {"archive":"WAI","entry_index":7,"size":20000,"opens":1,"reads":1,"read_bytes":10000,"seeks":0,"failed_operations":0,"trace_event_count":2}
        ]}
        out=module.profile(data)
        rows={(r["archive"],r["entry_index"]):r for r in out["entries"]}
        self.assertTrue(rows[("WAH",5)]["cache_candidate"])
        self.assertTrue(rows[("WAG",2)]["streaming_candidate"])
        self.assertTrue(rows[("WAG",2)]["seek_activity_candidate"])
        self.assertTrue(rows[("WAI",7)]["low_usage_candidate"])
        self.assertEqual(rows[("WAH",5)]["read_coverage_ratio"],0.5)
        self.assertEqual(out["summary"]["correlated_entries"],3)
        self.assertEqual(len(out["entries"]),3)
    def test_empty_report(self):
        result=module.profile({"entries":[]})
        self.assertEqual(result["summary"]["correlated_entries"],0)
        self.assertEqual(result["entries"],[])
if __name__=="__main__": unittest.main()
