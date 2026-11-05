import importlib.util
import json
from pathlib import Path
import tempfile
import unittest
import subprocess
import sys

spec = importlib.util.spec_from_file_location("analysis", Path(__file__).resolve().parents[1] / "analytics/analyze.py")
analysis = importlib.util.module_from_spec(spec)
spec.loader.exec_module(analysis)

class AnalyticsTests(unittest.TestCase):
    def run_history(self, records, extra=""):
        with tempfile.TemporaryDirectory() as folder:
            path = Path(folder) / "history-2026-11.jsonl"
            path.write_text("\n".join(json.dumps(x) for x in records)+"\n"+extra, encoding="utf-8")
            return analysis.analyze([path])

    def sample(self, at, level="N", provider="ukrainealarm", available=True):
        return dict(version=1, at=at, provider=provider, available=available, levels=level*27)

    def test_durations_and_observed_start(self):
        report = self.run_history([self.sample(100),self.sample(160,"R"),self.sample(220,"P"),self.sample(280)])
        row=report["rows"][0]
        self.assertEqual((row["clear"],row["red"],row["partial"],row["unknown"],row["starts"]),(60,60,60,0,1))

    def test_gap_is_not_assumed_clear(self):
        report=self.run_history([self.sample(100),self.sample(400,"R")])
        row=report["rows"][0]
        self.assertEqual((row["clear"],row["unknown"],row["starts"]),(90,210,0))

    def test_outage_ends_confirmation(self):
        report=self.run_history([self.sample(100,"R"),self.sample(120,available=False),self.sample(180)])
        row=report["rows"][0]
        self.assertEqual((row["red"],row["unknown"]),(20,60))

    def test_provider_switch_breaks_continuity(self):
        report=self.run_history([self.sample(100),self.sample(160,"R",provider="alerts.in.ua")])
        self.assertEqual(report["rows"][0]["unknown"],60)
        self.assertEqual(report["rows"][0]["starts"],0)

    def test_corrupt_and_malformed_records_are_reported(self):
        bad=self.sample(150); bad["levels"]="N"
        report=self.run_history([self.sample(100),bad,self.sample(200)],extra="{broken\n")
        self.assertEqual(report["rejected"],2)
        self.assertEqual(report["observations"],2)

    def test_duplicate_time_uses_latest_observation(self):
        report=self.run_history([self.sample(100),self.sample(100,"R"),self.sample(160,"R")])
        self.assertEqual(report["rows"][0]["red"],60)
        self.assertEqual(report["rows"][0]["starts"],0)

    def test_same_second_outage_boundary_prevents_false_start(self):
        report=self.run_history([self.sample(100),self.sample(120,available=False),self.sample(120,"R"),self.sample(160,"R")])
        self.assertEqual(report["rows"][0]["starts"],0)
        self.assertEqual((report["rows"][0]["clear"],report["rows"][0]["red"]),(20,40))

    def test_no_extrapolation_after_last_sample(self):
        report=self.run_history([self.sample(100,"R")])
        self.assertEqual(report["window_seconds"],0)
        self.assertEqual(report["rows"][0]["red"],0)

    def test_html_escapes_metadata_and_handles_empty_history(self):
        report=self.run_history([])
        regions=[dict(iso=f"T{i}",name="<script>alert(1)</script>") for i in range(27)]
        html=analysis.render_html(report,regions)
        self.assertNotIn("<script>alert(1)</script>",html)
        self.assertIn("&lt;script&gt;",html)
        self.assertIn("Немає спостережень",html)

    def test_cli_exports_same_totals_to_html_csv_and_json(self):
        with tempfile.TemporaryDirectory(prefix="AirAlertMap history ") as folder:
            root=Path(folder); output=root / "Звіти з пробілами"
            records=[self.sample(100),self.sample(160,"Y"),self.sample(220)]
            (root / "history-2026-11.jsonl").write_text("\n".join(map(json.dumps,records)),encoding="utf-8")
            run=subprocess.run([sys.executable,analysis.__file__,"--history",str(root),"--output",str(output),"--region","UA-30"],capture_output=True,text=True)
            self.assertEqual(run.returncode,0,run.stderr)
            report=json.loads((output / "history.json").read_text(encoding="utf-8"))
            self.assertEqual(report["rows"][0]["threat"],60)
            self.assertEqual(len(report["rows"]),27)
            self.assertIn('class="selected"',(output / "history.html").read_text(encoding="utf-8"))
            self.assertTrue((output / "history.csv").read_text(encoding="utf-8-sig").startswith("iso,name,"))

    def test_unavailable_sample_with_misleading_levels_stays_unknown(self):
        report=self.run_history([self.sample(100,"N",available=False),self.sample(160)])
        self.assertEqual(report["rows"][0]["unknown"],60)
        self.assertEqual(report["rows"][0]["clear"],0)

if __name__ == "__main__":
    unittest.main()
