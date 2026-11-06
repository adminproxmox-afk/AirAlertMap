# Local history storage maintenance

History retention is an explicit user choice. The default policy is **0: keep
all observations**. The application offers 90, 180 or 365 days only after the
user confirms cleanup now and on later runs. Native code stores this choice in
`history-policy.txt`, independently of preferences. The Python helper does not
read or change policy. Passing `--days 0`, omitting days for prune, or supplying
any other retention period fails without pruning.

The native storage panel measures only canonical observation files and
catalogs. ZIP export includes those same files and leaves originals in place.
Exports can be saved outside the history and application directories, for
example in Desktop or Downloads. Exports can contain historical territory
identifiers and provider observations; they never include preferences, keys,
retry state, generated reports, cache, browser profiles or policy.

## Helper contract

`analytics/manage_history.py` uses only the Python standard library and can run
under isolated Python (`python -I`). `--history` is the application's data root;
canonical files are directly inside it, and reports are inside its `reports`
subdirectory. `--output` is a native-generated JSON result filename in that
reports directory. `--now` is an integer Unix timestamp in UTC seconds from
0 through 9999999999. Both actions require it, even though export does not use a
retention cutoff.

```text
python -I analytics/manage_history.py --action archive --history HISTORY_ROOT --output HISTORY_ROOT/reports/storage-RESULT.json --destination EXPORT.zip --now UTC_SECONDS
python -I analytics/manage_history.py --action prune --history HISTORY_ROOT --output HISTORY_ROOT/reports/storage-RESULT.json --days 90 --now UTC_SECONDS
```

Prune accepts `--days 90`, `--days 180` or `--days 365` and rejects
`--destination`. Archive requires `--destination` and rejects `--days`.
The native host launches Python directly with isolated arguments and its
existing credential-stripped environment; it does not construct a shell command.

The result has exactly these fields, with no paths, history data, exception
messages or credentials:

```json
{"version":1,"ok":true,"action":"prune","removedRecords":12,"removedFiles":1,"archiveFiles":0,"error":""}
```

Exit status is zero only when the action and result write succeed. Counts are
nonnegative integers; `archiveFiles` counts observation files plus catalogs in
the ZIP, and removal counts describe a successful prune. Unsuccessful actions
report zero counts. The helper writes an initial failure result before starting
maintenance, then atomically replaces it with the final result. This checks
result write access before changes. If the output path is unsafe, unreachable
or becomes unwritable, a fresh result cannot always be produced; the native host
must also check child exit status and result identity.

| `error` | Meaning |
| --- | --- |
| empty string | Success. |
| `invalid_arguments` | Missing, inconsistent or unsupported CLI arguments, retention days or timestamp. |
| `unsafe_path` | A link/reparse point, nonregular canonical input, unsafe result path, or prohibited ZIP destination. |
| `invalid_history` | A malformed, oversized or unsupported observation was found; originals are preserved. |
| `io_error` | An input, staging, export or result preflight operation failed, or an input changed during preparation. |
| `commit_failed` | A staged prune could not be committed; rollback was attempted. |

## Allowed files and paths

The only source names are `history-YYYY-MM.jsonl` with month 01 through 12 and
`catalog-` followed by exactly 16 lowercase hexadecimal characters and `.json`.
The helper enumerates only direct children. Matching directories, symbolic
links and Windows reparse points cause failure. Existing ancestors of the root,
inputs, result and archive paths are checked for links/reparse points as well.
No record field, including catalog metadata, is interpreted as a path.

ZIP members are simple source basenames. Archive copies raw bytes without
parsing observations, so exporting damaged history is possible. It includes
every canonical catalog even when there are no observations. Empty history
produces a valid empty ZIP. The ZIP is built in a temporary file beside the
destination and replaces that destination only after every source is copied
and its identity is rechecked. Failures leave an existing export intact.
The destination must end in `.zip`, lie outside both the history root and the
application installation, and must not alias a selected source or result file.

Results may be written only to `.json` files in a `reports` directory; within the
history root this must be its direct `reports` directory. Results cannot use a
canonical source basename, overwrite app installation files, or overwrite a
linked file. Normal native calls always use `HISTORY_ROOT/reports/storage-*.json`.

## Retention and failure handling

The cutoff is `now - days * 86400`. Retention uses each observation's timestamp,
including files from a partial month, rather than the month in its filename.
Every sample at or after cutoff is retained, including future timestamps within
the supported integer range. The latest sample before cutoff is retained only
when it is within 90 seconds of cutoff. At equal timestamps, the last sample in
stable filename and line order is retained. This preserves the short evidence
interval at a report boundary without extending older confirmations.

Every nonblank line must be UTF-8 JSON with the legacy version 1 or 2 observation
fields: integer timestamp within the supported range, known provider, boolean
availability, and exactly 27 levels from `N`, `R`, `P`, `Y`, `?`. Duplicate JSON
keys, nonfinite numbers, unsupported fields' malformed JSON, and lines larger
than 2 MiB cause a safe failure. Additional valid JSON fields are allowed and
retained bytes are copied unchanged, including their whitespace, Unicode and
line endings. Blank lines are allowed. No fields are expanded or dereferenced.

All selected observation files are validated before mutations, and replacement
files plus original backups are fully staged and flushed first. If preparation
fails, no original changes. Unchanged files retain their original bytes; files
whose observations all expire are removed. Catalogs are never deleted, so
remaining records cannot lose their catalog references through this cleanup.

Commit uses atomic per-file replacements and attempts rollback after an ordinary
write or deletion failure. If the disk also refuses rollback, the helper keeps
the `.history-maintenance-*` staging directory with original backups and reports
`commit_failed`; it does not delete the remaining originals to clean up staging.
Multiple files do not form a filesystem transaction. If the child is terminated
during commit, some files may already have applied retention, while every
individual visible file remains complete and staged originals remain available.
Hidden staging directories are outside the canonical input allowlist.

Native code serializes maintenance with analytics and pauses observation writes
for its duration. The map remains live, and writes resume with fresh evidence
after maintenance. This coordination is required: the helper checks input
identity but does not implement a cross-process writer lock. Automatic cleanup
is permitted only for an explicitly saved nonzero retention policy, at most once
per day in the running process.

## Verification

```text
python -m unittest discover -s tests -p test_history_storage.py
python -m unittest discover -s tests -p "test_*.py"
```

Tests use unique temporary directories and synthetic records. They cover raw ZIP
round trips, catalogs and empty archives, private-file exclusions, destination
failures, partial months, 90-second boundaries across months, future data,
invalid records and oversized lines, canonical names, linked files, staged
commit rollback and backup preservation when rollback itself fails. They never
read actual application data, credentials or the user's LocalAppData history.
