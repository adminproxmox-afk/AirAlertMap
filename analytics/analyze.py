"""Analyze local AirAlertMap observations, without network access or dependencies."""
import argparse
import csv
from datetime import datetime, timezone
from html import escape
import json
import os
from pathlib import Path
import sys

LEVELS = {"N": "clear", "R": "red", "P": "partial", "Y": "threat", "?": "unknown"}

def valid_record(record):
    return (isinstance(record, dict) and record.get("version") == 1
            and type(record.get("at")) is int and 0 <= record["at"] <= 9999999999
            and record.get("provider") in ("ukrainealarm", "alerts.in.ua")
            and type(record.get("available")) is bool
            and isinstance(record.get("levels"), str) and len(record["levels"]) == 27
            and set(record["levels"]) <= LEVELS.keys())

def analyze(paths):
    records, rejected = [], 0
    for path in sorted(paths):
        with Path(path).open(encoding="utf-8", errors="replace") as source:
            for line in source:
                if not line.strip():
                    continue
                try:
                    record = json.loads(line)
                except (ValueError, RecursionError):
                    rejected += 1
                    continue
                if not valid_record(record):
                    rejected += 1
                    continue
                records.append(record)
    records.sort(key=lambda r: r["at"])
    # Stable ordering preserves outage boundaries even at the same timestamp.
    rows = [dict(index=i, clear=0, red=0, partial=0, threat=0, unknown=0, starts=0) for i in range(27)]
    for previous, current in zip(records, records[1:]):
        delta = current["at"] - previous["at"]
        same_source = previous["provider"] == current["provider"]
        confirmed = min(delta, 90) if previous["available"] and same_source else 0
        for i, row in enumerate(rows):
            level = previous["levels"][i]
            row[LEVELS[level]] += confirmed
            row["unknown"] += delta - confirmed
            if (same_source and 0 < delta <= 90 and previous["available"] and current["available"]
                    and level == "N" and current["levels"][i] in "RPY"):
                row["starts"] += 1
    return dict(rows=rows, rejected=rejected, observations=len(records),
                first=records[0]["at"] if records else None,
                last=records[-1]["at"] if records else None,
                window_seconds=records[-1]["at"]-records[0]["at"] if records else 0)

def duration(seconds):
    hours, rest = divmod(seconds, 3600)
    minutes, seconds = divmod(rest, 60)
    return f"{hours} год {minutes:02} хв {seconds:02} с"

def stamp(value):
    return datetime.fromtimestamp(value, timezone.utc).strftime("%d.%m.%Y %H:%M:%S UTC") if value is not None else "—"

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
<p>Це історія спостережень вашого застосунку, а не офіційний архів. Тривалість оцінюється між опитуваннями; одне спостереження підтверджує не більше 90 секунд. Після останнього запису час не додається. Перерви зв’язку та зміна джерела позначаються як невідомі. Переходи — лише помічені зміни від відсутності тривоги до тривоги або загрози.</p>
<div class="table-wrap"><table><thead><tr><th>Регіон</th><th>Повна тривога</th><th>Часткова</th><th>Попередня загроза</th><th>Без тривоги</th><th>Немає даних</th><th>Переходи</th><th>Покриття даними</th></tr></thead><tbody>{''.join(table)}</tbody></table></div>
<footer>Розраховано локально на Python · Час у UTC · CSV і JSON збережено поруч зі звітом.</footer></html>'''

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
    with (output / "history.csv.tmp").open("w", newline="", encoding="utf-8-sig") as target:
        writer = csv.DictWriter(target, fieldnames=["iso", "name", "clear", "red", "partial", "threat", "unknown", "starts"])
        writer.writeheader()
        writer.writerows({key: row[key] for key in writer.fieldnames} for row in enriched)
    os.replace(output / "history.csv.tmp", output / "history.csv")

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--history", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--region")
    args = parser.parse_args()
    try:
        regions = json.loads((Path(__file__).resolve().parents[1] / "data/regions.json").read_text(encoding="utf-8"))
        if len(regions) != 27 or (args.region and args.region not in {r["iso"] for r in regions}):
            raise ValueError("Invalid region metadata or selected region")
        if not args.history.is_dir():
            raise ValueError("History directory does not exist")
        summary = analyze(args.history.glob("history-*.jsonl"))
        write_reports(summary, regions, args.output, args.region)
    except (OSError, ValueError, KeyError) as error:
        print(f"Cannot create history report: {error}", file=sys.stderr)
        return 1
    print(f"Report created: {summary['observations']} observations, {summary['rejected']} rejected records")
    return 0

if __name__ == "__main__":
    raise SystemExit(main())
