import importlib.util, pathlib, unittest
ROOT=pathlib.Path(__file__).resolve().parents[1]
spec=importlib.util.spec_from_file_location("afs_runtime_binding",ROOT/"afs_runtime_binding.py")
mod=importlib.util.module_from_spec(spec);spec.loader.exec_module(mod)

class RuntimeBindingTests(unittest.TestCase):
 def test_candidate_only_and_no_ids(self):
  candidates={"entries":[{"archive":"wag","entry_index":2,"offset":100,"size":400,
   "sha256":"d"*64,"family_candidate":"texture_candidate","confidence":"high","evidence":"TIM2 signature"}]}
  profile={"entries":[{"archive":"WAG","entry_index":2,"access_score":12}]}
  intel={"entries":[{"archive":"WAG","entry_index":2,"load_strategy":"CACHE","strategy_reason":"profile rule"}]}
  out=mod.bind(candidates,profile,intel); r=out["entries"][0]
  self.assertEqual(r["resource_type_suggestion"],"Texture")
  self.assertEqual(r["load_strategy_suggestion"],"Cache")
  self.assertEqual(r["status"],"candidate_only")
  self.assertIsNone(r["logical_resource_id"])
  self.assertEqual(out["summary"]["verified_ids"],0)
 def test_missing_evidence_is_safe_default(self):
  out=mod.bind({"entries":[{"archive":"WAH","entry_index":7,"family_candidate":"unresolved"}]})
  r=out["entries"][0]
  self.assertEqual(r["resource_type_suggestion"],"Unknown")
  self.assertEqual(r["load_strategy_suggestion"],"OnDemand")
  self.assertEqual(r["strategy_confidence"],"default_safety")
if __name__=="__main__": unittest.main()
