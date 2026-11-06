"""Analyze local AirAlertMap observations, without network access or dependencies."""
import argparse
import csv
from collections import deque
from datetime import datetime, timezone
from html import escape
import json
import os
from pathlib import Path
import re
import sys

LEVELS = {"N": "clear", "R": "red", "P": "partial", "Y": "threat", "?": "unknown"}
MAX_LINE_BYTES = 2 * 1024 * 1024
MAX_CATALOG_BYTES = 16 * 1024 * 1024
NUMERIC_ID = re.compile(r"[0-9]{1,10}\Z")
CATALOG_ID = re.compile(r"[0-9a-f]{16}\Z")
TOTAL_FIELDS = ("clear", "red", "partial", "threat", "unknown", "starts")

def totals():
    return dict.fromkeys(TOTAL_FIELDS, 0)

def valid_record(record):
    return (isinstance(record, dict) and type(record.get("version")) is int and record["version"] in (1, 2)
            and type(record.get("at")) is int and 0 <= record["at"] <= 9999999999
            and record.get("provider") in ("ukrainealarm", "alerts.in.ua")
            and type(record.get("available")) is bool
            and isinstance(record.get("levels"), str) and len(record["levels"]) == 27
            and set(record["levels"]) <= LEVELS.keys())

def read_catalog(folder, digest):
    """Read only a digest-named file beside history; never use a recorded path."""
    if not isinstance(digest, str) or not CATALOG_ID.fullmatch(digest):
        return None
    path = folder / f"catalog-{digest}.json"
    try:
        if path.resolve().parent != folder.resolve():
            return None
        with path.open("rb") as source:
            raw = source.read(MAX_CATALOG_BYTES + 1)
        if len(raw) > MAX_CATALOG_BYTES:
            return None
        value = 14695981039346656037
        for byte in raw:
            value = ((value ^ byte) * 1099511628211) & 0xffffffffffffffff
        if f"{value:016x}" != digest:
            return None
        catalog = json.loads(raw.decode("utf-8"))
        if (not isinstance(catalog, dict) or type(catalog.get("version")) is not int
                or catalog["version"] != 1 or catalog.get("provider") != "ukrainealarm"):
            return None
        entries = catalog.get("territories")
        if not isinstance(entries, list) or not 0 < len(entries) <= 20000:
            return None
        indexed = {}
        for index, entry in enumerate(entries):
            if not isinstance(entry, dict):
                return None
            identifier, parent = entry.get("id"), entry.get("parent")
            if (not isinstance(identifier, str) or not NUMERIC_ID.fullmatch(identifier)
                    or identifier in indexed or not isinstance(parent, str)
                    or (parent and not NUMERIC_ID.fullmatch(parent))
                    or type(entry.get("oblast")) is not int or not -1 <= entry["oblast"] < 27):
                return None
            for field, maximum in (("name",512), ("type",32)):
                text = entry.get(field)
                if (not isinstance(text, str) or not text.strip() or len(text) > maximum
                        or any(ord(char) < 32 or 0xd800 <= ord(char) <= 0xdfff for char in text)):
                    return None
            indexed[identifier] = (index, {key: entry[key] for key in ("id", "name", "type", "oblast")})
        return len(entries), indexed
    except (OSError, ValueError, RecursionError, RuntimeError):
        return None

def add_interval(selected, daily, lower, upper, level):
    """Split a half-open interval at UTC midnight without losing seconds."""
    while lower < upper:
        end = min(upper, (lower // 86400 + 1) * 86400)
        day = datetime.fromtimestamp(lower, timezone.utc).strftime("%Y-%m-%d")
        row = daily.setdefault(day, dict(day=day, **totals()))
        row[LEVELS[level]] += end - lower
        selected[LEVELS[level]] += end - lower
        lower = end

def analyze(paths, since=None, territory=None):
    if territory == "":
        territory = None
    if territory is not None and (not isinstance(territory, str) or not NUMERIC_ID.fullmatch(territory)):
        raise ValueError("Territory must be a numeric provider ID of 1 to 10 digits")
    records, rejected = [], 0
    catalogs = {}
    selected = dict(id=territory, name=territory, type="Unknown", oblast=-1, **totals(), daily=[], events=[]) if territory is not None else None
    for path in sorted(paths):
        path = Path(path)
        with path.open("rb") as source:
            while True:
                line = source.readline(MAX_LINE_BYTES + 1)
                if not line:
                    break
                if len(line) > MAX_LINE_BYTES:
                    while line and not line.endswith(b"\n"):
                        line = source.readline(MAX_LINE_BYTES + 1)
                    rejected += 1
                    continue
                if not line.strip():
                    continue
                try:
                    record = json.loads(line)
                except (ValueError, UnicodeError, RecursionError):
                    rejected += 1
                    continue
                if not valid_record(record):
                    rejected += 1
                    continue
                record.pop("_territory_metadata", None)
                record["_territory_level"] = "?"
                if (selected is not None and record["version"] == 2 and record["provider"] == "ukrainealarm"
                        and record["available"]):
                    digest = record.get("catalog")
                    if isinstance(digest, str) and CATALOG_ID.fullmatch(digest):
                        key = (path.parent, digest)
                        if key not in catalogs:
                            catalogs[key] = read_catalog(path.parent, digest)
                        catalog = catalogs[key]
                        if catalog is not None and territory in catalog[1]:
                            index, metadata = catalog[1][territory]
                            record["_territory_metadata"] = metadata
                            levels = record.get("territoryLevels")
                            if isinstance(levels, str) and len(levels) == catalog[0] and set(levels) <= LEVELS.keys():
                                record["_territory_level"] = levels[index]
                # Keep only evidence used by the calculation. Compact native
                # snapshots contain every territory; retaining those strings
                # for a year of observations would dominate report memory.
                evidence = {key: record[key] for key in ("at", "provider", "available", "levels", "_territory_level")}
                if "_territory_metadata" in record:
                    evidence["_territory_metadata"] = record["_territory_metadata"]
                records.append(evidence)
    records.sort(key=lambda r: r["at"])
    if selected is not None:
        for record in records:
            if "_territory_metadata" in record:
                selected.update(record["_territory_metadata"])
    # Stable ordering preserves outage boundaries even at the same timestamp.
    if records and since is not None and since > records[-1]["at"]:
        records = []
    first = max(records[0]["at"], since or 0) if records else None
    last = records[-1]["at"] if records else None
    rows = [dict(index=i, **totals()) for i in range(27)]
    daily = {}
    events = deque(maxlen=200)
    for previous, current in zip(records, records[1:]):
        if current["at"] < first:
            continue
        lower = max(previous["at"], first)
        delta = current["at"] - lower
        same_source = previous["provider"] == current["provider"]
        confirmed = max(0, min(current["at"], previous["at"] + 90) - lower) if previous["available"] and same_source else 0
        for i, row in enumerate(rows):
            level = previous["levels"][i]
            row[LEVELS[level]] += confirmed
            row["unknown"] += delta - confirmed
            if (same_source and 0 < current["at"] - previous["at"] <= 90 and previous["available"] and current["available"]
                    and level == "N" and current["levels"][i] in "RPY"):
                row["starts"] += 1
        if selected is not None:
            level = previous["_territory_level"]
            add_interval(selected, daily, lower, lower + confirmed, level)
            add_interval(selected, daily, lower + confirmed, current["at"], "?")
            current_level = current["_territory_level"]
            if (same_source and 0 < current["at"] - previous["at"] <= 90
                    and previous["available"] and current["available"] and level != "?"
                    and current_level != "?" and level != current_level):
                events.append(dict(at=current["at"], previous=level, level=current_level))
                if level == "N" and current_level in "RPY":
                    selected["starts"] += 1
                    day = datetime.fromtimestamp(current["at"], timezone.utc).strftime("%Y-%m-%d")
                    daily.setdefault(day, dict(day=day, **totals()))["starts"] += 1
    if selected is not None:
        selected["daily"] = [daily[day] for day in sorted(daily)]
        selected["events"] = list(events)
    return dict(rows=rows, rejected=rejected, observations=sum(r["at"] >= first for r in records),
                first=first, last=last, window_seconds=last-first if records else 0, territory=selected)

def duration(seconds):
    hours, rest = divmod(seconds, 3600)
    minutes, seconds = divmod(rest, 60)
    return f"{hours} год {minutes:02} хв {seconds:02} с"

def stamp(value):
    return datetime.fromtimestamp(value, timezone.utc).strftime("%d.%m.%Y %H:%M:%S UTC") if value is not None else "—"

def render_territory(selected):
    if selected is None:
        return ""
    headings = "<th>Повна тривога</th><th>Часткова</th><th>Попередня загроза</th><th>Без тривоги</th><th>Немає даних</th><th>Початки</th>"
    def cells(row):
        return "".join(f"<td>{duration(row[key])}</td>" for key in ("red", "partial", "threat", "clear", "unknown")) + f'<td>{row["starts"]}</td>'
    daily = "".join(f'<tr><th scope="row">{row["day"]}</th>{cells(row)}</tr>' for row in selected["daily"])
    events = "".join(f'<tr><td>{stamp(event["at"])}</td><td>{event["previous"]}</td><td>{event["level"]}</td></tr>' for event in selected["events"])
    return f'''<section><h2>{escape(selected["name"])} · ID {escape(selected["id"])}</h2>
<p>{escape(selected["type"])} · Історичні назви та типи взято зі збережених каталогів постачальника.</p>
<div class="table-wrap"><table><thead><tr>{headings}</tr></thead><tbody><tr>{cells(selected)}</tr></tbody></table></div>
<h3>За днями UTC</h3><div class="table-wrap"><table><thead><tr><th>Дата UTC</th>{headings}</tr></thead><tbody>{daily}</tbody></table></div>
<h3>Останні підтверджені зміни стану</h3><p>N — без тривоги; R — повна; P — часткова; Y — попередня загроза. Показано до 200 змін між сусідніми підтвердженими відповідями. Невідомі стани не створюють подій.</p>
<div class="table-wrap"><table><thead><tr><th>Час UTC</th><th>Попередній стан</th><th>Новий стан</th></tr></thead><tbody>{events}</tbody></table></div></section>'''

def render_html(summary, regions, selected=None):
    table = []
    for row, region in zip(summary["rows"], regions):
        total = summary["window_seconds"]
        coverage = 100*(total-row["unknown"])/total if total else 0
        active = row["red"] + row["partial"] + row["threat"]
        width = 100*active/total if total else 0
        chosen = ' class="selected"' if region["iso"] == selected else ""
        cells = "".join(f"<td>{escape(duration(row[key]))}</td>" for key in ("red", "partial", "threat", "clear", "unknown"))
        table.append(f'<tr{chosen}><th scope="row">{escape(region["name"])}</th>{cells}'
                     f'<td>{row["starts"]}</td><td>{coverage:.1f}%<div class="bar"><span style="width:{width:.2f}%"></span></div></td></tr>')
    message = "Немає спостережень. Залиште програму відкритою для накопичення історії." if not summary["observations"] else (
        "Потрібно щонайменше два спостереження для розрахунку тривалості." if not summary["window_seconds"] else "")
    return f'''<!doctype html><html lang="uk"><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>Історія тривог · AirAlertMap</title><style>
:root{{color-scheme:dark;font:15px "Segoe UI",sans-serif;background:#101820;color:#e9f0f4}}body{{max-width:1500px;margin:auto;padding:40px 28px}}header{{border-bottom:1px solid #344653;padding-bottom:25px}}small{{color:#85c6bd;letter-spacing:.12em}}h1{{font-size:36px;margin:12px 0}}p{{line-height:1.65;color:#b4c4ce;max-width:1050px}}.summary{{display:flex;gap:40px;flex-wrap:wrap;padding:25px 0}}.summary strong{{display:block;font:28px Consolas,monospace;color:#fff}}.summary span{{color:#afc2ce}}.table-wrap{{overflow:auto}}table{{width:100%;border-collapse:collapse;font-size:13px}}td,th{{text-align:left;padding:14px 12px;border-bottom:1px solid #263743;white-space:nowrap}}thead{{background:#1c2b35;color:#b9cdd6}}tbody th{{font-weight:500}}.selected{{background:#233c43}}.bar{{height:3px;background:#34414b;margin-top:7px}}.bar span{{height:100%;display:block;background:#e9aa51}}.notice{{padding:15px 20px;background:#22323d;border-left:3px solid #e9aa51}}footer{{margin-top:25px;color:#91a7b5}}@media print{{:root{{color-scheme:light;background:white;color:black}}p,footer{{color:#333}}}}</style>
<header><small>AIRALERTMAP / ЛОКАЛЬНА ІСТОРІЯ</small><h1>Спостереження за тривогами</h1>
<p>{stamp(summary["first"])} — {stamp(summary["last"])}</p></header>
<div class="summary"><div><strong>{summary["observations"]}</strong><span>спостережень</span></div><div><strong>{duration(summary["window_seconds"])}</strong><span>між першим і останнім записом</span></div><div><strong>{summary["rejected"]}</strong><span>пошкоджених записів пропущено</span></div></div>
{f'<p class="notice">{message}</p>' if message else ''}
<p>Це історія спостережень вашого застосунку, а не офіційний архів. Тривалість оцінюється між опитуваннями; одне спостереження підтверджує не більше 90 секунд. Після останнього запису час не додається. Перерви зв’язку та зміна джерела позначаються як невідомі. Для окремої території старі записи та відсутні або пошкоджені каталоги також означають невідомий стан. Початки — лише помічені зміни від відсутності тривоги до тривоги або загрози.</p>
{render_territory(summary.get("territory"))}
<div class="table-wrap"><table><thead><tr><th>Регіон</th><th>Повна тривога</th><th>Часткова</th><th>Попередня загроза</th><th>Без тривоги</th><th>Немає даних</th><th>Переходи</th><th>Покриття даними</th></tr></thead><tbody>{''.join(table)}</tbody></table></div>
<footer>Розраховано локально на Python · Час у UTC · CSV і JSON збережено поруч зі звітом. Області: history.csv; вибрана територія: territory.csv, territory-daily.csv, territory-events.csv.</footer></html>'''

def write_csv(output, name, fields, rows):
    temporary = output / (name + ".tmp")
    with temporary.open("w", newline="", encoding="utf-8-sig") as target:
        writer = csv.DictWriter(target, fieldnames=fields, extrasaction="ignore")
        writer.writeheader()
        writer.writerows(rows)
    os.replace(temporary, output / name)

def write_reports(summary, regions, output, selected=None):
    output = Path(output)
    output.mkdir(parents=True, exist_ok=True)
    enriched = [{**row, "iso": region["iso"], "name": region["name"]} for row, region in zip(summary["rows"], regions)]
    for name, content in {
        "history.html": render_html(summary, regions, selected),
        "history.json": json.dumps({**summary, "rows": enriched}, ensure_ascii=False, indent=2),
    }.items():
        temporary = output / (name+".tmp")
        temporary.write_text(content, encoding="utf-8")
        os.replace(temporary, output / name)
    write_csv(output, "history.csv", ["iso", "name", *TOTAL_FIELDS], enriched)
    child = summary.get("territory")
    write_csv(output, "territory.csv", ["id", "name", "type", "oblast", *TOTAL_FIELDS], [child] if child else [])
    identity = {key: child[key] for key in ("id", "name")} if child else {}
    write_csv(output, "territory-daily.csv", ["id", "name", "day", *TOTAL_FIELDS],
              [{**identity, **row} for row in child["daily"]] if child else [])
    write_csv(output, "territory-events.csv", ["id", "name", "at", "previous", "level"],
              [{**identity, **row} for row in child["events"]] if child else [])

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--history", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--region")
    parser.add_argument("--territory", help="Numeric UkraineAlarm territory ID (1 to 10 digits)")
    parser.add_argument("--since", type=int)
    args = parser.parse_args()
    try:
        regions = json.loads((Path(__file__).resolve().parents[1] / "data/regions.json").read_text(encoding="utf-8"))
        if len(regions) != 27 or (args.region and args.region not in {r["iso"] for r in regions}):
            raise ValueError("Invalid region metadata or selected region")
        if not args.history.is_dir():
            raise ValueError("History directory does not exist")
        if args.since is not None and args.since < 0:
            raise ValueError("Period start must be a nonnegative UTC timestamp")
        summary = analyze(args.history.glob("history-*.jsonl"), since=args.since, territory=args.territory)
        write_reports(summary, regions, args.output, args.region)
    except (OSError, ValueError, KeyError) as error:
        print(f"Cannot create history report: {error}", file=sys.stderr)
        return 1
    print(f"Report created: {summary['observations']} observations, {summary['rejected']} rejected records")
    return 0

if __name__ == "__main__":
    raise SystemExit(main())
