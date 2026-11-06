"""Storage maintenance uses synthetic fixtures in unique temporary directories only."""
import importlib.util
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
import zipfile
from unittest.mock import patch


HELPER = Path(__file__).resolve().parents[1] / "analytics/manage_history.py"
NOW = 1800000000
CUTOFF = NOW - 90 * 86400


class HistoryStorageTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix="AirAlertMap storage test ")
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.history = self.root / "history"
        self.history.mkdir()
        self.output = self.history / "reports" / "storage-result.json"
        self.destination = self.root / "export.zip"

    def sample(self, at, **extra):
        return dict(version=1, at=at, provider="ukrainealarm", available=True,
                    levels="N" * 27, **extra)

    def write(self, records, name="history-2026-11.jsonl"):
        path = self.history / name
        path.write_bytes(b"".join(json.dumps(r, ensure_ascii=False).encode("utf-8") + b"\n" for r in records))
        return path

    def run_helper(self, action="prune", days="90", **changes):
        args = dict(action=action, history=self.history, output=self.output, now=NOW)
        if action == "archive":
            args["destination"] = self.destination
        elif days is not None:
            args["days"] = days
        args.update(changes)
        command = [sys.executable, "-I", str(HELPER)]
        for key, value in args.items():
            command.extend(["--" + key, str(value)])
        process = subprocess.run(command, capture_output=True, timeout=15)
        self.assertTrue(self.output.exists(), "helper must return safe JSON even on errors")
        result = json.loads(self.output.read_text(encoding="utf-8"))
        self.assertEqual(set(result), {"version", "ok", "action", "removedRecords", "removedFiles", "archiveFiles", "error"})
        self.assertEqual(result["version"], 1)
        self.assertEqual(process.returncode == 0, result["ok"])
        self.assertEqual(process.stdout, b"")
        self.assertEqual(process.stderr, b"")
        return result

    def records(self, path):
        return [json.loads(line) for line in path.read_bytes().splitlines() if line.strip()]

    def test_prune_partial_month_keeps_recent_and_future(self):
        path = self.write([self.sample(CUTOFF - 100), self.sample(CUTOFF), self.sample(NOW + 100)])
        result = self.run_helper()
        self.assertTrue(result["ok"])
        self.assertEqual((result["removedRecords"], result["removedFiles"]), (1, 0))
        self.assertEqual([r["at"] for r in self.records(path)], [CUTOFF, NOW + 100])

    def test_no_implicit_prune_without_days(self):
        path = self.write([self.sample(100)])
        before = path.read_bytes()
        result = self.run_helper(days=None)
        self.assertEqual(result["error"], "invalid_arguments")
        self.assertEqual(path.read_bytes(), before)

    def test_zero_days_never_deletes(self):
        path = self.write([self.sample(100)])
        self.assertFalse(self.run_helper(days="0")["ok"])
        self.assertTrue(path.exists())

    def test_only_supported_retention_windows(self):
        for days in ("1", "91", "-90", "1.5"):
            with self.subTest(days=days):
                self.assertEqual(self.run_helper(days=days)["error"], "invalid_arguments")

    def test_180_and_365_days(self):
        for days in (180, 365):
            with self.subTest(days=days):
                cutoff = NOW - days * 86400
                path = self.write([self.sample(cutoff - 100), self.sample(cutoff)])
                self.assertEqual(self.run_helper(days=str(days))["removedRecords"], 1)
                self.assertEqual([r["at"] for r in self.records(path)], [cutoff])

    def test_keeps_only_latest_boundary_sample_across_months(self):
        old = self.write([self.sample(CUTOFF - 90)], "history-2026-08.jsonl")
        new = self.write([self.sample(CUTOFF - 30), self.sample(CUTOFF + 1)])
        result = self.run_helper()
        self.assertEqual((result["removedRecords"], result["removedFiles"]), (1, 1))
        self.assertFalse(old.exists())
        self.assertEqual(len(self.records(new)), 2)

    def test_boundary_90_seconds_is_inclusive(self):
        path = self.write([self.sample(CUTOFF - 91), self.sample(CUTOFF - 90)])
        self.assertEqual(self.run_helper()["removedRecords"], 1)
        self.assertEqual(self.records(path)[0]["at"], CUTOFF - 90)

    def test_boundary_uses_timestamp_not_filename_or_line_order(self):
        path = self.write([self.sample(CUTOFF - 5), self.sample(CUTOFF - 40), self.sample(CUTOFF - 200)])
        self.assertEqual(self.run_helper()["removedRecords"], 2)
        self.assertEqual(self.records(path)[0]["at"], CUTOFF - 5)

    def test_removes_fully_expired_file(self):
        path = self.write([self.sample(100)])
        result = self.run_helper()
        self.assertFalse(path.exists())
        self.assertEqual((result["removedRecords"], result["removedFiles"]), (1, 1))

    def test_catalogs_never_pruned(self):
        catalog = self.history / "catalog-0123456789abcdef.json"
        catalog.write_bytes(b"arbitrary catalog bytes")
        self.write([self.sample(100)])
        self.assertTrue(self.run_helper()["ok"])
        self.assertEqual(catalog.read_bytes(), b"arbitrary catalog bytes")

    def test_extra_fields_and_raw_utf8_bytes_are_preserved(self):
        path = self.write([self.sample(100)])
        raw = ('  {"version":2,"at":' + str(NOW) + ',"provider":"alerts.in.ua","available":false,"levels":"' + "?" * 27 + '","catalog":"../../keys","extra":"Україна"}  \r\n').encode("utf-8")
        path.write_bytes(path.read_bytes() + raw)
        self.assertTrue(self.run_helper()["ok"])
        self.assertEqual(path.read_bytes(), raw)

    def test_malformed_later_file_aborts_all_prepared_changes(self):
        good = self.write([self.sample(100)], "history-2026-01.jsonl")
        bad = self.history / "history-2026-12.jsonl"
        bad.write_bytes(b'{"at": "private-file-path"}\n')
        before = good.read_bytes(), bad.read_bytes()
        result = self.run_helper()
        self.assertEqual(result["error"], "invalid_history")
        self.assertEqual((good.read_bytes(), bad.read_bytes()), before)
        self.assertNotIn("private-file-path", self.output.read_text())

    def test_oversized_line_preserves_original(self):
        path = self.history / "history-2026-11.jsonl"
        raw = b" " * (2 * 1024 * 1024 + 1) + b"\n"
        path.write_bytes(raw)
        self.assertEqual(self.run_helper()["error"], "invalid_history")
        self.assertEqual(path.read_bytes(), raw)

    def test_invalid_record_types_preserve_original(self):
        for field, value in (("version", True), ("version", 3), ("at", True), ("at", -1),
                             ("at", 10000000000), ("at", "100"), ("levels", "N" * 26),
                             ("levels", "X" * 27), ("provider", "bad"), ("available", 1)):
            with self.subTest(field=field, value=value):
                record = self.sample(100)
                record[field] = value
                path = self.write([record])
                before = path.read_bytes()
                self.assertEqual(self.run_helper()["error"], "invalid_history")
                self.assertEqual(path.read_bytes(), before)

    def test_timestamp_upper_bound_is_retained(self):
        path = self.write([self.sample(9999999999)])
        self.assertTrue(self.run_helper()["ok"])
        self.assertTrue(path.exists())

    def test_utf16_input_is_rejected_without_loss(self):
        path = self.history / "history-2026-11.jsonl"
        raw = json.dumps(self.sample(100)).encode("utf-16")
        path.write_bytes(raw)
        self.assertEqual(self.run_helper()["error"], "invalid_history")
        self.assertEqual(path.read_bytes(), raw)

    def test_nonfinite_and_duplicate_json_fields_are_rejected(self):
        path = self.write([self.sample(100)])
        raw = path.read_bytes()
        for invalid in (raw.replace(b'"at": 100', b'"at": 100, "at": 1'),
                        raw.replace(b'"at": 100', b'"at": 100, "extra": NaN')):
            path.write_bytes(invalid)
            self.assertEqual(self.run_helper()["error"], "invalid_history")
            self.assertEqual(path.read_bytes(), invalid)

    def test_canonical_names_only(self):
        for name in ("history-2026-00.jsonl", "history-2026-13.jsonl", "history-26-11.jsonl", "history-2026-11.jsonl.bak", "catalog-0123456789ABCDEF.json", "preferences.json", "history-policy.txt"):
            (self.history / name).write_bytes(b"private data")
        self.assertTrue(self.run_helper()["ok"])
        self.assertTrue(self.run_helper(action="archive")["ok"])
        with zipfile.ZipFile(self.destination) as archive:
            self.assertEqual(archive.namelist(), [])

    def test_archive_raw_files_and_catalog_only_flat_basenames(self):
        path = self.write([self.sample(100)])
        catalog = self.history / "catalog-0123456789abcdef.json"
        catalog.write_bytes(b"{catalog}")
        for name in ("preferences.json", "history-policy.txt", "keys.bin", "retry.json"):
            (self.history / name).write_bytes(b"secret")
        (self.history / "reports").mkdir()
        (self.history / "reports" / "history.json").write_bytes(b"report")
        result = self.run_helper(action="archive")
        self.assertEqual(result["archiveFiles"], 2)
        with zipfile.ZipFile(self.destination) as archive:
            self.assertEqual(set(archive.namelist()), {path.name, catalog.name})
            self.assertEqual(archive.read(path.name), path.read_bytes())
            self.assertEqual(archive.read(catalog.name), b"{catalog}")

    def test_archive_with_only_catalog_and_empty_history(self):
        (self.history / "catalog-0123456789abcdef.json").write_bytes(b"{}")
        self.assertEqual(self.run_helper(action="archive")["archiveFiles"], 1)

    def test_archive_empty_is_valid_zip(self):
        self.assertEqual(self.run_helper(action="archive")["archiveFiles"], 0)
        with zipfile.ZipFile(self.destination) as archive:
            self.assertEqual(archive.namelist(), [])

    def test_archive_does_not_require_parseable_records(self):
        (self.history / "history-2026-11.jsonl").write_bytes(b"damaged original")
        self.assertTrue(self.run_helper(action="archive")["ok"])
        with zipfile.ZipFile(self.destination) as archive:
            self.assertEqual(archive.read("history-2026-11.jsonl"), b"damaged original")

    def test_archive_rejects_destination_anywhere_inside_history(self):
        self.write([self.sample(100)])
        self.assertEqual(self.run_helper(action="archive", destination=self.history / "reports" / "export.zip")["error"], "unsafe_path")

    def test_archive_rejects_source_and_policy_overwrite(self):
        path = self.write([self.sample(100)])
        policy = self.root / "history-policy.txt"
        policy.write_bytes(b"90")
        for destination in (path, policy, HELPER, HELPER.parents[1] / "data" / "regions.json"):
            with self.subTest(destination=destination):
                self.assertEqual(self.run_helper(action="archive", destination=destination)["error"], "unsafe_path")
        self.assertEqual(policy.read_bytes(), b"90")

    def test_archive_destination_failure_preserves_existing_archive(self):
        self.destination.mkdir()
        result = self.run_helper(action="archive")
        self.assertFalse(result["ok"])
        self.assertTrue(self.destination.is_dir())
        self.assertEqual(list(self.root.glob("*.tmp")), [])

    def test_missing_history_errors_without_creating_it(self):
        missing = self.root / "missing"
        self.assertEqual(self.run_helper(history=missing)["error"], "io_error")
        self.assertFalse(missing.exists())

    def test_invalid_now_is_safe_and_preserves_history(self):
        path = self.write([self.sample(100)])
        for now in ("garbage", "-1", "10000000000", "True"):
            self.assertEqual(self.run_helper(now=now)["error"], "invalid_arguments")
            self.assertTrue(path.exists())

    def test_canonical_directory_is_rejected(self):
        (self.history / "history-2026-11.jsonl").mkdir()
        self.assertEqual(self.run_helper()["error"], "unsafe_path")

    def test_symlink_history_entry_is_not_followed(self):
        target = self.root / "outside.jsonl"
        target.write_bytes(b"private")
        link = self.history / "history-2026-11.jsonl"
        try:
            link.symlink_to(target)
        except OSError:
            self.skipTest("OS does not permit unprivileged symlink creation")
        self.assertEqual(self.run_helper(action="archive")["error"], "unsafe_path")
        self.assertEqual(target.read_bytes(), b"private")

    def test_empty_and_whitespace_files_do_not_disappear(self):
        path = self.history / "history-2026-11.jsonl"
        path.write_bytes(b"\n \r\n")
        self.assertTrue(self.run_helper()["ok"])
        self.assertEqual(path.read_bytes(), b"\n \r\n")

    def test_commit_failure_rolls_back_already_replaced_files(self):
        first = self.write([self.sample(100), self.sample(NOW)], "history-2026-01.jsonl")
        second = self.write([self.sample(100), self.sample(NOW)], "history-2026-02.jsonl")
        before = first.read_bytes(), second.read_bytes()
        spec = importlib.util.spec_from_file_location("manage_history", HELPER)
        module = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(module)
        replace = module.os.replace
        failed = False

        def fail_second(source, destination):
            nonlocal failed
            if Path(destination) == second and not failed:
                failed = True
                raise OSError("synthetic write failure with private details")
            return replace(source, destination)

        with patch.object(module.os, "replace", side_effect=fail_second):
            status = module.main(["--action", "prune", "--history", str(self.history), "--output", str(self.output), "--days", "90", "--now", str(NOW)])
        self.assertNotEqual(status, 0)
        self.assertEqual((first.read_bytes(), second.read_bytes()), before)
        self.assertEqual(json.loads(self.output.read_text())["error"], "commit_failed")

    def test_failed_rollback_keeps_original_backups_without_more_disk_writes(self):
        first = self.write([self.sample(100), self.sample(NOW)], "history-2026-01.jsonl")
        second = self.write([self.sample(100), self.sample(NOW)], "history-2026-02.jsonl")
        before = first.read_bytes()
        spec = importlib.util.spec_from_file_location("manage_history", HELPER)
        module = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(module)
        replace = module.os.replace
        first_replaced = False

        def disk_failure(source, destination):
            nonlocal first_replaced
            if Path(destination) == first:
                if first_replaced:
                    raise OSError("disk cannot restore backup")
                first_replaced = True
            elif Path(destination) == second:
                raise OSError("disk cannot commit second file")
            return replace(source, destination)

        with patch.object(module.os, "replace", side_effect=disk_failure), patch.object(module.shutil, "copytree", side_effect=OSError("disk cannot copy recovery data")):
            status = module.main(["--action", "prune", "--history", str(self.history), "--output", str(self.output), "--days", "90", "--now", str(NOW)])
        self.assertNotEqual(status, 0)
        backups = list(self.history.glob(".history-maintenance-*/*.original"))
        self.assertTrue(any(path.read_bytes() == before for path in backups), "failed rollback must retain original bytes")

    def test_archive_replace_failure_preserves_previous_export(self):
        self.write([self.sample(100)])
        self.destination.write_bytes(b"previous export")
        spec = importlib.util.spec_from_file_location("manage_history", HELPER)
        module = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(module)
        replace = module.os.replace

        def reject_export(source, destination):
            if Path(destination) == self.destination:
                raise OSError("cannot replace destination")
            return replace(source, destination)

        with patch.object(module.os, "replace", side_effect=reject_export):
            status = module.main(["--action", "archive", "--history", str(self.history), "--output", str(self.output), "--destination", str(self.destination), "--now", str(NOW)])
        self.assertNotEqual(status, 0)
        self.assertEqual(self.destination.read_bytes(), b"previous export")
        self.assertEqual(list(self.root.glob(".history-export-*")), [])

    def test_noop_prune_does_not_require_staging_disk_space(self):
        path = self.write([self.sample(NOW)])
        before = path.read_bytes()
        spec = importlib.util.spec_from_file_location("manage_history", HELPER)
        module = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(module)
        with patch.object(module.tempfile, "mkdtemp", side_effect=OSError("no disk space for staging")):
            status = module.main(["--action", "prune", "--history", str(self.history), "--output", str(self.output), "--days", "90", "--now", str(NOW)])
        self.assertEqual(status, 0)
        self.assertEqual(path.read_bytes(), before)
        result = json.loads(self.output.read_text())
        self.assertEqual((result["removedRecords"], result["removedFiles"]), (0, 0))


if __name__ == "__main__":
    unittest.main()
