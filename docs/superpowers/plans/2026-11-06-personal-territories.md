# Personal territories, history, map preferences and portable release

Goal: implement user-selected items 1, 3, 4, 5 and 6 from the project roadmap.
Architecture: keep C++Builder/x86 ASM, offline HTML UI and Python standard-library analytics. No notification feature requested. Preserve current aggregate history and credentials.

Decisions: up to 20 favorites identified by geometry key (State/District/Community + OCHA code); home must be a favorite. Preference JSON in LocalAppData, atomically written by C++, never browser storage. Persist selected territory, viewBox, detail mode, layer flags, theme, brightness and border scale. Debounce map gestures; commit immediately at pointerup and settings changes. Keep muted current palette as default.

Preference protocol: native state preferencesRevision (integer), preferences object {version:1,favorites:[key...],home:key-or-empty,selectedTerritory:key-or-empty,view:{x,y,w,h},detailLevel:'auto'|'State'|'District'|'Community',showLabels:boolean,showBorders:boolean,showStatuses:boolean,theme:'muted'|'graphite'|'midnight',brightness:60..120 integer,borderWidth:60..160 integer}. Geometry keys follow /^(State|District|Community):UA[0-9]{2,9}$/; max length32. View finite, x,y>=0,w31.25..1000,h=w*0.7, fits bounds1000x700. Command {version:1,type:'preferences',preferences:object}. Native state preferencesError string; failed save preserves stored state and surfaces error.

History protocol: native existing history-YYYY-MM.jsonl becomes version2, retains aggregate levels27. Confirmed UkraineAlarm lines additionally contain catalog (16 lowercase hex) and territoryLevels (one N/R/Y/P/? per catalog entry). Catalog files catalog-<digest>.json contain {version:1,provider:'ukrainealarm',territories:[{id,name,parent,type,oblast}]}. Stable FNV-1a64 over exact catalog UTF8; collision checked against bytes before reuse. Outages have no territory evidence. Python validates catalog and levels, treats old records/gaps/provider changes as unknown, never extrapolates >90s or beyond last sample.

Command history accepts optional territory numeric provider ID (1..10digits), missing/empty = oblast report. Native publishes historyTerritory. Python CLI --territory ID and report keeps original rows27 while adding territory:null or selected summary {id,name,type,oblast,red,partial,threat,clear,unknown,starts,daily:[{day,red,partial,threat,clear,unknown,starts}],events:[{at,previous,level}]}. Daily UTC dates; events only confirmed adjacent transitions, max200. Missing catalogs are unknown, not clear. CSV/HTML/JSON include selected report. Historical territory metadata comes from recorded catalogs.

Release: portable ZIP plus startup instructions, version0.4.0, real icon/versionresource, curated assets only, checksums. No user history, preferences, diagnostics, credentials or WebView profile in release. Python optional for map; missing Python shown inside history. Missing WebView2 shown clearly at startup. No unsolicited installs/network during launch.

Tasks:
- [x] Native preferences validation, atomic storage, command and state revision; tests invalid/range/duplicate values and restart.
- [x] Native v2 recording and catalogs; tests stable catalog, clear/outage, collision/corruption and credentials excluded.
- [x] Python selected territory history, daily totals, confirmed events, backwards compatibility and exports; meaningful tests.
- [x] Frontend favorites/home, persisted view/theme controls, selected territory history/charts; browser tests reload, drag, heartbeats and stale states.
- [x] Release icon/version, dependency UX and curated archive; verify manifest and isolated launch.
- [x] Native build/checks, Python/JS, browser smoke, portable archive integrity; independent review findings fixed.
- [ ] Final native window interaction check: user stopped Computer Use with physical Escape; no further app input issued.

Review focus: stale status in favorites; same-name territories; out-of-order preference acknowledgements overwriting drafts; interrupted history/catalog write; packaging private runtime data accidentally.

Verification: native build.ps1 passed all C++/ASM checks; extras passed Python31/JS21 plus native-Python interoperability and isolated interpreter launch. Frontend browser checks 21+11+27+17 passed. Actual release ZIP verified22curatedfiles,9HTMLassetreferences,PEversion0.4.0,x86andMAINICON.
