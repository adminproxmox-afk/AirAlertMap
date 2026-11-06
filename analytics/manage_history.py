"""Offline, stdlib-only maintenance of canonical AirAlertMap history files.

Native code owns user confirmation and exclusion of observation/report writers.
No recorded field is ever interpreted as a filesystem path.
"""
import argparse
import json
import os
from pathlib import Path
import re
import shutil
import stat
import sys
import tempfile
import zipfile


MAX_LINE_BYTES = 2 * 1024 * 1024
HISTORY_NAME = re.compile(r"history-[0-9]{4}-(?:0[1-9]|1[0-2])\.jsonl\Z")
CATALOG_NAME = re.compile(r"catalog-[0-9a-f]{16}\.json\Z")
APP_ROOT = Path(__file__).resolve().parents[1]


class MaintenanceError(Exception):
    def __init__(self, code):
        self.code = code


class SafeParser(argparse.ArgumentParser):
    def error(self, message):
        raise MaintenanceError("invalid_arguments")


def safe_path(value):
    """Reject links/reparse points on every existing component, including parents."""
    path = Path(value)
    if ".." in path.parts:
        raise MaintenanceError("unsafe_path")
    path = Path(os.path.abspath(path))
    for component in (*reversed(path.parents), path):
        try:
            info = component.lstat()
        except FileNotFoundError:
            continue
        if stat.S_ISLNK(info.st_mode) or getattr(info, "st_file_attributes", 0) & 0x400:
            raise MaintenanceError("unsafe_path")
    return path


def inside(path, directory):
    return path == directory or directory in path.parents


def regular(path):
    safe_path(path)
    info = path.lstat()
    if not stat.S_ISREG(info.st_mode):
        raise MaintenanceError("unsafe_path")
    return info


def identity(info):
    return info.st_dev, info.st_ino, info.st_size, info.st_mtime_ns


def open_source(path, expected):
    """Recheck identity at open; native holds the application writer exclusion."""
    regular(path)
    descriptor = os.open(path, os.O_RDONLY | getattr(os, "O_BINARY", 0) | getattr(os, "O_NOFOLLOW", 0))
    source = os.fdopen(descriptor, "rb")
    if identity(os.fstat(source.fileno())) != expected:
        source.close()
        raise MaintenanceError("io_error")
    return source


def inputs(folder):
    safe_path(folder)
    if not folder.is_dir():
        raise MaintenanceError("io_error")
    selected = []
    for path in sorted(folder.iterdir()):
        if HISTORY_NAME.fullmatch(path.name) or CATALOG_NAME.fullmatch(path.name):
            selected.append((path, identity(regular(path))))
    return selected


def reject_constant(value):
    raise ValueError("nonfinite")


def unique_fields(pairs):
    record = {}
    for key, value in pairs:
        if key in record:
            raise ValueError("duplicate")
        record[key] = value
    return record


def read_records(path, expected):
    with open_source(path, expected) as source:
        while True:
            raw = source.readline(MAX_LINE_BYTES + 1)
            if not raw:
                break
            if len(raw) > MAX_LINE_BYTES:
                raise MaintenanceError("invalid_history")
            if not raw.strip():
                yield raw, None
                continue
            try:
                record = json.loads(raw.decode("utf-8"), parse_constant=reject_constant, object_pairs_hook=unique_fields)
            except (ValueError, UnicodeError, RecursionError):
                raise MaintenanceError("invalid_history") from None
            if not (isinstance(record, dict)
                    and type(record.get("version")) is int and record["version"] in (1, 2)
                    and type(record.get("at")) is int and 0 <= record["at"] <= 9999999999
                    and record.get("provider") in ("ukrainealarm", "alerts.in.ua")
                    and type(record.get("available")) is bool
                    and isinstance(record.get("levels"), str) and len(record["levels"]) == 27
                    and set(record["levels"]) <= set("NRPY?")):
                raise MaintenanceError("invalid_history")
            yield raw, record["at"]


def check_unchanged(selected):
    for path, expected in selected:
        if identity(regular(path)) != expected:
            raise MaintenanceError("io_error")


def prune(folder, selected, days, now):
    histories = [(path, expected) for path, expected in selected if HISTORY_NAME.fullmatch(path.name)]
    cutoff = now - days * 86400
    boundary = None
    expired = {}
    # Complete validation before staging or changing any original. The stable
    # path/line order selects the last sample when timestamps are equal.
    for path, expected in histories:
        expired[path] = 0
        for line_number, (_, at) in enumerate(read_records(path, expected)):
            if at is not None and at < cutoff:
                expired[path] += 1
            if at is not None and cutoff - 90 <= at < cutoff:
                if boundary is None or at >= boundary[0]:
                    boundary = at, path, line_number
    changed = [(path, expected) for path, expected in histories
               if expired[path] > int(boundary is not None and boundary[1] == path)]
    if not changed:
        check_unchanged(histories)
        return 0, 0
    removed_records = removed_files = 0
    staging = Path(tempfile.mkdtemp(prefix=".history-maintenance-", dir=folder))
    preserve_backups = False
    try:
        changes = []
        for index, (path, expected) in enumerate(changed):
            staged = staging / (str(index) + ".new")
            backup = staging / (str(index) + ".original")
            removed = retained = 0
            with staged.open("wb") as target, backup.open("wb") as original:
                for line_number, (raw, at) in enumerate(read_records(path, expected)):
                    original.write(raw)
                    keep = at is None or at >= cutoff or (boundary is not None and (at, path, line_number) == boundary)
                    if keep:
                        target.write(raw)
                        retained += at is not None
                    else:
                        removed += 1
                target.flush()
                original.flush()
                os.fsync(target.fileno())
                os.fsync(original.fileno())
            if removed:
                changes.append((path, staged if retained else None, backup))
                removed_records += removed
                removed_files += retained == 0
        check_unchanged(histories)
        committed = []
        try:
            for path, staged, backup in changes:
                if staged is None:
                    path.unlink()
                else:
                    os.replace(staged, path)
                committed.append((path, backup))
        except OSError:
            # Backups precede every mutation. A normal commit failure restores
            # earlier replacements; abrupt process termination remains valid
            # per-file and may leave a partially applied retention operation.
            for path, backup in reversed(committed):
                try:
                    os.replace(backup, path)
                except OSError:
                    # Keep remaining backups for recovery if rollback itself
                    # cannot write. Do not silently erase the only original.
                    preserve_backups = True
            raise MaintenanceError("commit_failed") from None
    finally:
        if not preserve_backups:
            shutil.rmtree(staging)
    return removed_records, removed_files


def archive(folder, selected, destination, output):
    destination = safe_path(destination)
    if (inside(destination, folder) or inside(destination, APP_ROOT)
            or destination.suffix.lower() != ".zip" or destination == output):
        raise MaintenanceError("unsafe_path")
    if destination.exists():
        regular(destination)
        if any(os.path.samefile(destination, path) for path, _ in selected):
            raise MaintenanceError("unsafe_path")
    # The SaveDialog selects an existing directory; never create destinations
    # or traverse directories based on history record contents.
    descriptor, temporary = tempfile.mkstemp(prefix=".history-export-", suffix=".tmp", dir=destination.parent)
    os.close(descriptor)
    try:
        with zipfile.ZipFile(temporary, "w", compression=zipfile.ZIP_DEFLATED, allowZip64=True) as result:
            for path, expected in selected:
                with open_source(path, expected) as source, result.open(path.name, "w", force_zip64=True) as target:
                    shutil.copyfileobj(source, target, length=1024 * 1024)
        check_unchanged(selected)
        with open(temporary, "r+b") as finished:
            os.fsync(finished.fileno())
        safe_path(destination)
        os.replace(temporary, destination)
    finally:
        if os.path.exists(temporary):
            os.unlink(temporary)
    return len(selected)


def result_output(path, history):
    path = safe_path(path)
    # Only the native-generated reports result is writable within the data
    # root. The helper cannot overwrite policy, history, catalogs or app code.
    if (path.parent.name != "reports" or path.suffix != ".json"
            or (inside(path, history) and path.parent != history / "reports")
            or inside(path, APP_ROOT)
            or HISTORY_NAME.fullmatch(path.name) or CATALOG_NAME.fullmatch(path.name)):
        raise MaintenanceError("unsafe_path")
    if path.exists():
        regular(path)
        if path.stat().st_nlink != 1:
            raise MaintenanceError("unsafe_path")
    path.parent.mkdir(exist_ok=True)
    safe_path(path)
    return path


def write_result(path, value):
    safe_path(path)
    descriptor, temporary = tempfile.mkstemp(prefix=".storage-result-", suffix=".tmp", dir=path.parent)
    try:
        with os.fdopen(descriptor, "w", encoding="utf-8", newline="\n") as target:
            json.dump(value, target, separators=(",", ":"), ensure_ascii=True)
            target.write("\n")
            target.flush()
            os.fsync(target.fileno())
        os.replace(temporary, path)
    finally:
        if os.path.exists(temporary):
            os.unlink(temporary)


def main(argv=None):
    argv = sys.argv[1:] if argv is None else argv
    result = dict(version=1, ok=False, action="", removedRecords=0, removedFiles=0, archiveFiles=0, error="invalid_arguments")
    output = None
    try:
        # Recover only the safe output destination from otherwise bad CLI
        # arguments so native code still receives a bounded fixed result.
        fallback = SafeParser(add_help=False, allow_abbrev=False)
        fallback.add_argument("--history")
        fallback.add_argument("--output")
        fallback.add_argument("--action")
        partial, _ = fallback.parse_known_args(argv)
        if partial.action in ("archive", "prune"):
            result["action"] = partial.action
        if partial.history and partial.output:
            output = result_output(partial.output, safe_path(partial.history))
            write_result(output, result)  # Fail before maintenance if results cannot be saved.
        parser = SafeParser(add_help=False, allow_abbrev=False)
        parser.add_argument("--action", choices=("archive", "prune"), required=True)
        parser.add_argument("--history", required=True)
        parser.add_argument("--output", required=True)
        parser.add_argument("--destination")
        parser.add_argument("--days", choices=("90", "180", "365"))
        parser.add_argument("--now", required=True)
        args = parser.parse_args(argv)
        if (not re.fullmatch(r"[0-9]{1,10}", args.now) or int(args.now) > 9999999999
                or (args.action == "prune" and (args.days is None or args.destination is not None))
                or (args.action == "archive" and (args.destination is None or args.days is not None))):
            raise MaintenanceError("invalid_arguments")
        folder = safe_path(args.history)
        if output is None:
            raise MaintenanceError("unsafe_path")
        selected = inputs(folder)
        if args.action == "archive":
            result["archiveFiles"] = archive(folder, selected, args.destination, output)
        else:
            result["removedRecords"], result["removedFiles"] = prune(folder, selected, int(args.days), int(args.now))
        result.update(ok=True, error="")
    except MaintenanceError as error:
        result["error"] = error.code
    except (OSError, ValueError, OverflowError, RuntimeError):
        result["error"] = "io_error"
    if output is not None:
        try:
            write_result(output, result)
        except (OSError, MaintenanceError, ValueError):
            return 1
    return 0 if result["ok"] else 1


if __name__ == "__main__":
    sys.exit(main())
