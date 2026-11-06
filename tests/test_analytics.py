import importlib.util
import csv
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

    def test_period_clips_interval_without_losing_baseline(self):
        with tempfile.TemporaryDirectory() as folder:
            path=Path(folder)/"history-2026-11.jsonl"
            path.write_text("\n".join(json.dumps(x) for x in [self.sample(100),self.sample(160,"R"),self.sample(220)]),encoding="utf-8")
            report=analysis.analyze([path],since=130)
            self.assertEqual((report["first"],report["last"],report["window_seconds"]),(130,220,90))
            self.assertEqual((report["rows"][0]["clear"],report["rows"][0]["red"],report["rows"][0]["starts"]),(30,60,1))

    def test_period_does_not_extend_an_old_confirmation(self):
        with tempfile.TemporaryDirectory() as folder:
            path=Path(folder)/"history-2026-11.jsonl"
            path.write_text("\n".join(json.dumps(x) for x in [self.sample(100),self.sample(400)]),encoding="utf-8")
            report=analysis.analyze([path],since=250)
            self.assertEqual((report["rows"][0]["clear"],report["rows"][0]["unknown"]),(0,150))
            empty=analysis.analyze([path],since=500)
            self.assertEqual((empty["observations"],empty["window_seconds"]),(0,0))

class TerritoryAnalyticsTests(unittest.TestCase):
    sample = AnalyticsTests.sample
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.path = self.root / "history-2026-11.jsonl"
        self.entries = [dict(id="101", name="Однакова назва", parent="1", type="District", oblast=4),
                        dict(id="102", name="Однакова назва", parent="1", type="Community", oblast=4)]
        self.catalog = self.write_catalog(self.entries)

    def write_catalog(self, entries, **extra):
        data = json.dumps(dict(version=1, provider="ukrainealarm", territories=entries, **extra), ensure_ascii=False).encode("utf-8", errors="backslashreplace")
        value = 14695981039346656037
        for byte in data:
            value = ((value ^ byte) * 1099511628211) & 0xffffffffffffffff
        digest = f"{value:016x}"
        (self.root / f"catalog-{digest}.json").write_bytes(data)
        return digest

    def child(self, at, levels="NN", **changes):
        record = dict(self.sample(at), version=2, catalog=self.catalog, territoryLevels=levels)
        record.update(changes)
        return record

    def report(self, records, since=None, territory="101"):
        self.path.write_text("\n".join(map(json.dumps, records)), encoding="utf-8")
        return analysis.analyze([self.path], since=since, territory=territory)

    def test_selected_id_has_own_totals_despite_duplicate_name(self):
        report = self.report([self.child(100,"NR"), self.child(160,"RP"), self.child(220,"NY")])
        selected = report["territory"]
        self.assertEqual((selected["id"],selected["name"],selected["type"],selected["oblast"]), ("101","Однакова назва","District",4))
        self.assertEqual((selected["clear"],selected["red"],selected["starts"]), (60,60,1))
        other = self.report([self.child(100,"NR"),self.child(160,"RP"),self.child(220,"NY")],territory="102")["territory"]
        self.assertEqual((other["red"],other["partial"],other["clear"]), (60,60,0))
        self.assertEqual(report["rows"][0]["clear"],120)

    def test_old_records_and_provider_changes_are_unknown(self):
        selected = self.report([self.sample(100),self.child(160,"RN"), self.child(220,"RN",provider="alerts.in.ua"), self.child(280,"NN")])["territory"]
        self.assertEqual((selected["unknown"],selected["red"],selected["starts"]),(180,0,0))
        self.assertEqual(selected["events"],[])

    def test_missing_catalog_never_becomes_clear(self):
        selected = self.report([self.child(100,catalog="0"*16),self.child(160,catalog="0"*16)])["territory"]
        self.assertEqual((selected["unknown"],selected["clear"],selected["name"]),(60,0,"101"))

    def test_catalog_digest_and_duplicate_ids_are_validated(self):
        (self.root / f"catalog-{self.catalog}.json").write_text("{}",encoding="utf-8")
        selected = self.report([self.child(100),self.child(160)])["territory"]
        self.assertEqual((selected["unknown"],selected["clear"]),(60,0))
        self.catalog = self.write_catalog([self.entries[0],dict(self.entries[1],id="101")])
        selected = self.report([self.child(100),self.child(160)])["territory"]
        self.assertEqual((selected["unknown"],selected["name"]),(60,"101"))

    def test_bad_catalog_fields_make_evidence_unknown(self):
        for changes in ({"oblast":True},{"oblast":27},{"id":"../101"},{"name":""},{"parent":"../1"},{"type":[]}, {"name":"x"*513}, {"name":"\ud800"}):
            with self.subTest(changes=changes):
                self.catalog = self.write_catalog([dict(self.entries[0],**changes)])
                selected = self.report([self.child(100,"N"),self.child(160,"N")])["territory"]
                self.assertEqual((selected["unknown"],selected["clear"]),(60,0))

    def test_catalog_entry_count_is_bounded(self):
        self.catalog = self.write_catalog([dict(self.entries[0],id=str(i)) for i in range(20001)])
        selected = self.report([self.child(100,"N"*20001),self.child(160,"N"*20001)])["territory"]
        self.assertEqual((selected["unknown"],selected["name"]),(60,"101"))

    def test_oversized_and_invalid_utf8_lines_are_rejected_without_losing_next_sample(self):
        self.path.write_bytes(json.dumps(self.child(100)).encode() + b"\n" + b"x"*(2*1024*1024+1) + b"\n" + b'"\xff"\n' + json.dumps(self.child(160)).encode())
        report = analysis.analyze([self.path],territory="101")
        self.assertEqual((report["rejected"],report["observations"],report["territory"]["clear"]),(2,2,60))

    def test_legacy_and_missing_evidence_do_not_bridge_transition(self):
        records = [self.child(100),self.sample(120),self.child(150,"RN"),self.child(180,catalog="0"*16),self.child(210),self.child(240,"PN")]
        selected = self.report(records)["territory"]
        self.assertEqual((selected["clear"],selected["red"],selected["unknown"],selected["starts"]),(50,30,60,1))
        self.assertEqual(selected["events"],[dict(at=240,previous="N",level="P")])

    def test_duplicate_timestamp_and_current_unknown_have_no_confirmed_event(self):
        selected = self.report([self.child(100),self.child(100,"RN"),self.child(160,"?N"),self.child(220,"PN"),self.child(280,"NN")])["territory"]
        self.assertEqual((selected["red"],selected["partial"],selected["unknown"],selected["starts"]),(60,60,60,0))
        self.assertEqual(selected["events"],[dict(at=280,previous="P",level="N")])

    def test_bad_levels_and_catalog_path_preserve_aggregate(self):
        for changes in ({"territoryLevels":"N"},{"territoryLevels":"NZ"},{"catalog":"../outside"}, {"territoryLevels":[]}, {"catalog":None}):
            with self.subTest(changes=changes):
                report = self.report([self.child(100,**changes),self.child(160,**changes)])
                self.assertEqual((report["territory"]["unknown"],report["rows"][0]["clear"],report["rejected"]),(60,60,0))

    def test_record_cannot_supply_unvalidated_metadata_or_level(self):
        record = dict(self.sample(100),_territory_level="N",_territory_metadata=dict(id="101",name="Forged",type="State",oblast=2))
        selected = self.report([record,self.sample(160)])["territory"]
        self.assertEqual((selected["name"],selected["type"],selected["unknown"],selected["clear"]),("101","Unknown",60,0))

    def test_outage_duplicate_timestamp_and_gap_bound_confirmation(self):
        selected = self.report([self.child(100,"RN"),self.child(120,available=False),self.child(120,"NN"),self.child(180,"PN"),self.child(480,"RN")])["territory"]
        self.assertEqual((selected["red"],selected["clear"],selected["partial"],selected["unknown"],selected["starts"]),(20,60,90,210,1))
        self.assertEqual(selected["events"],[dict(at=180,previous="N",level="P")])

    def test_daily_utc_split_and_clipped_window(self):
        # Epoch 86400 is 1970-01-02 00:00:00 UTC.
        selected = self.report([self.child(86370,"RN"), self.child(86430,"NN"),self.child(86490,"YN"),self.child(86550,"NN")],since=86385)["territory"]
        self.assertEqual((selected["red"],selected["clear"],selected["threat"]),(45,60,60))
        self.assertEqual(selected["daily"],[dict(day="1970-01-01",clear=0,red=15,partial=0,threat=0,unknown=0,starts=0),dict(day="1970-01-02",clear=60,red=30,partial=0,threat=60,unknown=0,starts=1)])
        for key in ("clear","red","partial","threat","unknown","starts"):
            self.assertEqual(sum(day[key] for day in selected["daily"]),selected[key])
        clipped = self.report([self.child(100),self.child(400,"RN")],since=250)["territory"]
        self.assertEqual((clipped["clear"],clipped["unknown"]),(0,150))

    def test_events_only_confirmed_transitions_and_last_200(self):
        records = [self.child(100 + i*10,"RN" if i % 2 else "NN") for i in range(211)]
        selected = self.report(records)["territory"]
        self.assertEqual(len(selected["events"]),200)
        self.assertEqual(selected["events"][0],dict(at=210,previous="N",level="R"))
        self.assertEqual(selected["events"][-1],dict(at=2200,previous="R",level="N"))
        self.assertEqual(selected["starts"],105)

    def test_no_selected_territory_and_empty_missing_id(self):
        self.assertIsNone(self.report([],territory=None)["territory"])
        selected = self.report([])["territory"]
        self.assertEqual(selected,dict(id="101",name="101",type="Unknown",oblast=-1,clear=0,red=0,partial=0,threat=0,unknown=0,starts=0,daily=[],events=[]))
        selected = self.report([self.child(100,"RN")])["territory"]
        self.assertEqual((selected["red"],selected["unknown"],selected["events"]),(0,0,[]))
        self.assertEqual(self.report([self.child(100),self.child(160)],territory="999")["territory"]["unknown"],60)
        self.assertEqual(self.report([self.child(100),self.child(160)],since=200)["territory"]["unknown"],0)

    def test_selected_metadata_follows_catalog_id_across_reordering(self):
        first = self.child(100,"RN")
        self.catalog = self.write_catalog([self.entries[1],dict(self.entries[0],name="Нова назва")])
        selected = self.report([first,self.child(160,"NN"),self.child(220,"NR")])["territory"]
        self.assertEqual((selected["name"],selected["red"],selected["clear"],selected["starts"]),("Нова назва",60,60,1))
        self.assertEqual(selected["events"],[dict(at=160,previous="R",level="N"),dict(at=220,previous="N",level="R")])

    def test_cli_selected_exports_include_totals_days_events_and_escape_name(self):
        self.catalog = self.write_catalog([dict(self.entries[0],name='<img src=x onerror="alert(1)">')])
        self.report([self.child(100,"N"),self.child(160,"R"),self.child(220,"N")])
        output = self.root / "reports"
        run = subprocess.run([sys.executable,analysis.__file__,"--history",str(self.root),"--output",str(output),"--territory","101"],capture_output=True,text=True)
        self.assertEqual(run.returncode,0,run.stderr)
        summary = json.loads((output / "history.json").read_text(encoding="utf-8"))
        self.assertEqual(summary["territory"]["red"],60)
        with (output / "territory.csv").open(encoding="utf-8-sig",newline="") as source:
            rows = list(csv.DictReader(source))
        self.assertEqual((rows[0]["id"],rows[0]["red"],rows[0]["starts"]),("101","60","1"))
        with (output / "territory-daily.csv").open(encoding="utf-8-sig",newline="") as source:
            self.assertEqual(list(csv.DictReader(source))[0]["clear"],"60")
        with (output / "territory-events.csv").open(encoding="utf-8-sig",newline="") as source:
            self.assertEqual(list(csv.DictReader(source))[0]["level"],"R")
        html = (output / "history.html").read_text(encoding="utf-8")
        self.assertIn("&lt;img",html)
        self.assertNotIn("<img src=x",html)
        self.assertIn("1970-01-01",html)
        # A missing ID still replaces the previous selected report.
        regions = [dict(iso=f"T{i}",name=str(i)) for i in range(27)]
        analysis.write_reports(self.report([],territory="999"),regions,output)
        with (output / "territory.csv").open(encoding="utf-8-sig",newline="") as source:
            self.assertEqual(list(csv.DictReader(source))[0]["id"],"999")

    def test_invalid_selected_id_is_rejected(self):
        for territory in ("../1","1.0","1"*11,True,"１２"):
            with self.subTest(territory=territory), self.assertRaises(ValueError):
                self.report([],territory=territory)

if __name__ == "__main__":
    unittest.main()
