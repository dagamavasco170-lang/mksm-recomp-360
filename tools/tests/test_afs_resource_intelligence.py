import importlib.util,pathlib,unittest
ROOT=pathlib.Path(__file__).resolve().parents[1]
spec=importlib.util.spec_from_file_location("afs_resource_intelligence",ROOT/"afs_resource_intelligence.py")
mod=importlib.util.module_from_spec(spec);spec.loader.exec_module(mod)
class IntelligenceTests(unittest.TestCase):
 def test_budget_and_strategy_decisions(self):
  data={"entries":[
   {"archive":"WAG","entry_index":1,"size_bytes":100,"opens":5,"reads":8,"read_bytes":200,"seeks":0,"access_score":90,"cache_candidate":True,"streaming_candidate":False,"seek_activity_candidate":False},
   {"archive":"WAH","entry_index":2,"size_bytes":2000000,"opens":1,"reads":1,"read_bytes":1000,"seeks":0,"access_score":60,"cache_candidate":False,"streaming_candidate":True,"seek_activity_candidate":False},
   {"archive":"WAI","entry_index":3,"size_bytes":400000,"opens":1,"reads":2,"read_bytes":200000,"seeks":5,"access_score":30,"cache_candidate":False,"streaming_candidate":False,"seek_activity_candidate":True}
  ]}
  result=mod.recommend(data,cache_budget=1000,ram_budget=10000)
  m={(r["archive"],r["entry_index"]):r for r in result["entries"]}
  self.assertEqual(m[("WAG",1)]["load_strategy"],"CACHE")
  self.assertEqual(m[("WAH",2)]["load_strategy],"STREAM")
  self.assertEqual(m[("WAI",3)]["load_strategy],"SEEK_AWARE")
  self.assertEqual(result["summary"]["cache_allocated_bytes"],100)
 def test_cache_budget_exhaustion(self):
  base={"size_bytes":256,"opens":2,"reads":2,"read_bytes":100,"access_score":1,"cache_candidate":True,"streaming_candidate":False,"seek_activity_candidate":False}
  result=mod.recommend({"entries":[dict(base,archive="A",entry_index=1),dict(base,archive="B",entry_index=2)]},cache_budget=256)
  self.assertEqual(result["summary"]["cache_entries"],1)
  self.assertEqual(result["summary"]["on_demand_entries"],1)
if __name__=="__main__":unittest.main()
