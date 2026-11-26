# CHANGELOG.md

## 2026-11-26 — Automatic release workflow (`src/tools/github-release/`)

- New: `src/tools/github-release/release.yml` (installs to
- `.github/workflows/release.yml` via `enable.ps1` next to it — `src/`
- forbids dot-directories so the workflow cannot live at `.github/` in the
- workspace). Tag push `v*.*.*` builds (VS2022 x64, `windows-2022`), runs
- `ctest --preset ci` before packaging (red suite ships nothing), builds the
- `release` preset (Qt UI best-effort), gates on license/provenance +
- forbidden-content scan, and publishes zip + SHA256SUMS + build-info
- (`-rc`/hyphen tags become pre-releases; `workflow_dispatch` dry-runs
- without publishing). Validated here by YAML parse (15 steps, all with
- run/uses, ci→test→release→gate→package→release ordering).
- Docs: `BUILDING.md` gains an "Automatic releases" section;
- `STATUS_EVIDENCE.md` gains the unwatched-run row; folder README covers
- enable/use/status.
- LIMITATION: no Actions run observed yet — no green log exists. Record the
- first green run URL in STATUS_EVIDENCE before treating any release as
- verified. Not compiled here either (no MSVC/SDK in this workspace).

## 2026-11-26 — C++ increment 27 (driver-error trip latch from read upstream CODE + md)

Code read this round (payloads already on disk; re-read in full now):
- victory-fork `crates/victory-gpu/src/drvwatch.rs` @ `ceb9e1f7a018` FULL —
- `seen()` (every entry counted, first trips the switch for the rest of the
- run) + `tripped()` / `errors()` + the first-trips test vector; the
- EventLog subscription (`EvtSubscribe` to nvlddmkm/amdkmdag/amdwddmg/igfx*/
- Display, `VICTORY_DRIVER_WATCH=0` opt-out, once-guard, 4125 Auto HDR
- lesson) read and REFUSED — no OS event-log collection in this tree.
- Xbox2Windows `docs/startup-runtime.md` (null-binding + per-reason census
- sections, re-read to confirm the totals/delta/overflow discipline the
- latch will sit beside) + five-star `docs/exe-intake.md` (report, not
- exploration — re-read to keep this increment report-shaped, not
- execution-shaped).
- No code copied. All behavior below is independent implementation under MIT.

Docs read this round:
- src/docs/STATUS_EVIDENCE.md section A (queue + evidence reconcile).
- src/docs/ARCHITECTURE.md + src/docs/BUILDING.md (target + preset story).

Implemented:
- `telemetry/driver_watch`: new `DriverWatch` (noteError counts +1 and
- returns true only on the trip; tripped/errors/reset; mutex-guarded,
- portable, never throws). Wired into `sb_telemetry`. No OS calls, no
- statics: each owner holds its own latch (upstream uses process statics).
- No caller yet — the Diagnostics page will surface it once an honest
- source exists (same rule as the increment-25 compare helper).
- Tests: +9 checks (fresh, first-trips, tripped-once, second-no-trip,
- counts-all, keeps-counting to 5, reset, retrip + independent instance).
- LIMITATION unchanged: still not compiled here; Windows build + ctest required.

## 2026-11-26 — C++ increment 26 (Shader Manager surfaces the failure tally)

- Code read: none new (GUI surfacing of the increment-23 `graphics/shader_tally`
- FailTally backend; no batch translator run exists, so only the accounting travels).
- Docs read this round:
- src/docs/STATUS_EVIDENCE.md section A (queue reconcile),
- src/docs/ARCHITECTURE.md (C++ target + prototype mapping),
- src/docs/BUILDING.md (Windows build + presets story),
- src/AGENTS.md + src/README.md + src/ScarlettBridge_CPP_Runtime_and_GUI_Agent_Brief.md §8.2/§8.3 (Shader Manager page contract + GUI behavior rules).
- Implemented:
- `src/cpp/src/ui/backend_status`: new portable `failTallyLine()`
- (one-line total + reasons) + `failTallyReportLines()` (summary first, then
- up to maxDetail most-frequent-first report lines, mirroring the
- `perReasonDetailLines` 1+maxDetail budget). No Qt, never throws, unit-tested.
- `src/cpp/src/ui/ui_shell`: the shell now owns a real `gfx::FailTally` and
- the Shader Manager page shows it under the cache stats (summary + up to 8
- report lines via the new helpers, refreshed by the existing Refresh button;
- an honest "no shader batch failures recorded yet (no batch translator run)"
- note when the total is zero). No other page changed (one behavior per change).
- Tests: +7 checks (zero line/report, counts, summary-first + first-example,
- detail cap, merged-tally counts).
- LIMITATION unchanged: still not compiled here; Windows build + ctest required.

## 2026-11-26 — C++ increment 25 (declared-output compare from read upstream CODE + md)

Code read this round (payload already on disk; re-read in full now):
- Xbox2Windows `src/startup_stubs/api_trace.h` @ `8c9f168f4746` FULL —
- decide(): Unknown (no contract) / Unreadable (nothing comparable) /
- Same (every compared output matched) / Changed, plus the partial flag;
- format(): "?", "unreadable", "same", "changed:<args>" with ",partial";
- caller-sized buffers compared within the 64-byte bound; only declared
- outputs ever read. The provider/slot table and any tracing hooks were
- read and REFUSED — no providers exist in this tree.
- No code copied. All behavior below is independent implementation under MIT.

Docs read this round:
- src/docs/STATUS_EVIDENCE.md section A (queue + evidence reconcile).
- Xbox2Windows `docs/non-windows-hosts.md` (fully read earlier; comparison
- discipline re-applied here): negative controls must fail the same
- comparison, mismatches stay mismatches, skips stay skips — so "same"
- never claims a write was skipped, and anything uncompared is flagged
- partial instead of silently passing.

Implemented:
- `telemetry/output_compare`: `decideCompare` (null contract → Unknown;
- zero/uncompared widths skipped; widths past the snapshot bound compare
- within it; partial when checked != declared) + `formatCompare` (exact
- "?" / "unreadable" / "same" / "changed:<arg0>[,<arg1>]" + ",partial"
- shapes with the caller's own argument numbers). Portable, no threads,
- never throws. Wired into `sb_telemetry`. No in-tree caller yet (no
- providers here) — the helper awaits an honest trace source.
- Tests: +17 checks (null/unknown, unreadable + text, same + text,
- changed first/second arg + texts, same/changed partial + texts,
- zero-bytes unreadable, bound clamp, both-changed text).
- LIMITATION unchanged: still not compiled here; Windows build + ctest required.

## 2026-11-26 — C++ increment 24 (Setup exports the scan report)

Docs read this round (payloads already on disk; no new CODE pulled):
- five-star `docs/exe-intake.md` @ `340249e6514b` FULL — "a report, not
- exploration": one command writes `<exe>.report.md` (for reading) and
- `<exe>.report.json` (for tooling); import gaps first by call sites; the
- report stays in the out folder, game files stay out of the repo.
- Xbox2Windows `docs/non-windows-hosts.md` @ `e868b1f2d9b9a` (evidence +
- handoff sections) — record the exact commands run and observed results;
- state plainly what did NOT run locally; translation-layer runs are for
- finding problems, never reported as verification.
- Behavior taken: the Setup page scan now exports a redacted human-readable
- text report (the for-reading half next to the on-screen counts), reusing
- the guarded outside-repo export path; gaps/errors are reported verbatim,
- never smoothed over. No game files, no binaries, no paths beyond the
- shown (possibly redacted) dir.

Implemented:
- `src/cpp/src/ui/backend_status`: new portable `folderReportText()` (dir +
- entries/bytes/hashed + scan_ms + ok/failed status; repeats the scan's
- shown dir). No Qt, never throws, unit-tested.
- `src/cpp/src/ui/ui_shell`: Setup page keeps the last scan and gains an
- "Export scan report…" button (no-scan-yet and in-tree refusals reported
- honestly; exports counted + traced + logged). No other page changed (one
- behavior per change).
- Tests: +6 checks (redacted dir, counts, ms, ok status, failed status +
- error, plain dir passthrough).
- LIMITATION unchanged: still not compiled here; Windows build + ctest required.

## 2026-11-26 — C++ increment 23 (shader failure tally from newly read upstream CODE)

Code read (payload already on disk under scratch/code/files7):
- victory-fork `apps/victory/src/translate.rs` @ `ceb9e1f7a018` — the
- `Tally` failure contract re-read in full this round: `fail(reason,
- example)` (first example wins; list mode records one
- "<example>  <reason>" line per failure), `merge` (counts summed, first
- examples kept, lines appended), and the failure report shape (list lines
- sorted ascending; failures sorted most-frequent-first with ties in
- reason order, printed as "  <count:7>  <reason>  (e.g. <example>)").
- The translator stage counters (containers/programs/emitted/validated/
- pipelines tallies) were read and REFUSED — this tree has no batch
- translator run, so only the failure accounting travels.
- No code copied. All behavior below is independent implementation under MIT.

Implemented:
- `graphics/shader_tally`: new `FailTally` (list flag, fail/merge,
- totals, per-reason counts, sorted list lines, most-frequent-first
- report with the upstream width/order rules). Portable, no threads,
- never throws. Wired into `sb_graphics`.
- Tests: +13 checks (empty/unseen, first-example-wins, totals,
- non-list silence, list flag + sorted lines, merge keeps-first + sums,
- merged lines, merge-empty identity, exact report order + width-7
- format, tied-count reason order).
- LIMITATION unchanged: still not compiled here; Windows build + ctest required.

## 2026-11-26 — C++ increment 22 (Diagnostics probes engine-instance names)

- Code read: none new (GUI surfacing of the increment-21 `stats.rs`
- parse_instance backend; OS counter collection stays refused).
- `src/cpp/src/ui/backend_status`: new portable `engineInstanceLine()`
- (one-line meaning of a counter instance name: `pid=<pid> kind=<kind>`,
- or a not-a-name note; names the rule only — there are no live engine
- figures). No Qt, never throws, unit-tested.
- `src/cpp/src/ui/ui_shell`: Diagnostics page gains an engine probe row
- (instance-name field prefilled with a format example, live result on
- every keystroke like the adapter hint) under an honest no-live-figures
- note. No other page changed (one behavior per change).
- Tests: +4 checks (3D / Compute_0 wordings, not-a-name note, empty-kind
- marker).
- LIMITATION unchanged: still not compiled here; Windows build + ctest required.

## 2026-11-26 — C++ increment 21 (engine-counter aggregation from newly read upstream CODE)

Code read (payloads already on disk under scratch/code/files7 + files5):
- victory-fork `crates/victory-gpu/src/stats.rs` @ `ceb9e1f7a018` FULL
- (Stats shape, NOW look, 1 s sampling thread, system/process/thread times,
- PDH engine counters, parse_instance + busiest with exact unit vectors,
- per-kind log line) — the pure parse/aggregate vectors are mirrored below;
- the OS collection (PDH handles, GetSystem/Process/ThreadTimes, ToolHelp
- snapshots, memory queries, sampling thread) is refused (no OS stat
- collection in this tree).
- victory-fork `apps/victory/src/translate.rs` @ `ceb9e1f7a018` (Tally
- fail/merge: reason → (count, first example), list-mode line shape,
- per-stage counter routing) — sampled; the fail-tally shape is noted for a
- later shader-batch increment (title/shader-IR-coupled counting refused).
- Xbox2Windows `src/startup_stubs/api_trace.h` @ `8c9f168f4746` FULL
- (declared-output table, decide() Unknown/Unreadable/Same/Changed +
- partial, 64-byte bounded snapshots, "changed:0,1,partial" format) —
- read only; the declared-output compare helper is noted for later (no
- provider calls in this tree).
- No code copied. All behavior below is independent implementation under MIT.

Implemented:
- `telemetry/engine_stats`: `parseEngineInstance` ("pid_<pid>_..._engtype_
- <kind>" → process + kind; strict numeric pid with 32-bit range check;
- repeated "engtype_" ends the kind where the upstream split read ends it)
- + `busiestEngineKind` (per-kind sums over caller-supplied samples, whole
- card or one process; winner is the highest sum, 100 at most, 0 when
- nothing matches; deterministic key order). No OS calls, no threads.
- Wired into `sb_telemetry` via CMake.
- Tests: +10 checks (the four upstream parse vectors + missing-kind +
- pid-overflow guards; whole-card/pid-filter/missing-process sums + 100
- clamp on the upstream engine fixture).
- LIMITATION unchanged: still not compiled here; Windows build + ctest required.

## 2026-11-26 — C++ increment 20 (Controls page surfaces scripted input)

- `src/cpp/src/ui/backend_status`: new portable `scriptSummaryLine()`
- (entry/rejection counts + base + span, empty note), `scriptStateLine()`
- (hex buttons + six axis values), `scriptDetailLines()` (rejection lines
- capped to N, item text capped to 80 glyphs). No Qt, never throws,
- unit-tested. DEMO only: no device, no presenter.
- `src/cpp/src/ui/ui_shell`: Controls page gains a script box (default
- `1@0+0.3, 2@0.5, lt=1@1.0`), a Parse button, a summary label (counts +
- first 8 rejections), and a DEMO state label that replays the parsed
- script at the synthetic tick once Start is pressed (live `pollOnce`
- stays Unsupported and is labelled as such). No other page changed (one
- behavior per change).
- Tests: +9 checks (empty/clean/reject summaries, button/trigger states,
- rejection reasons, detail cap, long-text truncation).
- LIMITATION unchanged: still not compiled here; Windows build + ctest required.

## 2026-11-26 — C++ increment 19 (scripted input from newly read upstream CODE)

Code read (via GitHub REST contents API at pinned SHAs into
scratch/code/files8 + raw8):
- Xbox2Windows `tests/startup_gameinput_script.cpp` @ `8c9f168f4746` FULL
  (GameInput.dll stand-in plays the final entry of a long script; the
  41-entry/725-char and 33-entry cases behind the env-var + entry caps) —
  behavior only; the COM/DLL harness itself is refused (OS-coupled).
- Xbox2Windows `tests/startup_gameinput_script_syntax.cpp` @ `8c9f168f4746`
  (parser checked directly: legacy sscanf differential, 20-row rejection
  table with exact reasons, base=start, 256-entry + 4095-char limits;
  playback checked through the stand-in with a controllable clock and
  picture signal) — the parse vectors are mirrored below; playback through
  a presenter is refused (no picture signal in this tree).
- victory-fork `crates/victory-gpu/src/copystat.rs` @ `ceb9e1f7a018`
  (per-line-reset copy counters, block_class bucketing + exact vectors,
  command_key summarization, big-block listing, counting allocator) —
  read only; refused as title-memory/GPU-copy-coupled (our telemetry
  counters are cumulative and our smoke performs no copies).
- No code copied. All behavior below is independent implementation under MIT.

Implemented:
- `media/input_script`: portable scripted gamepad input — `parseInputScript`
  (button `<hex>@<s>[+<d>]`, axes `lt/rt` in [0,1] + `lx/ly/rx/ry` in
  [-1,1], relative `@+` times, `base=poll|start` first-item rule, 256-entry
  cap, 1-based rejections with the exact upstream reason strings; wait /
  mark / group / end items are kept as "unsupported directive (waits, marks
  and groups need a presenter)" rejections instead of running) +
  `playInputScript` (button OR, later axis wins) + `firstInputStart` /
  `lastInputEnd`. No OS, no devices. Unit-tested, wired into `sb_media`.
- Tests: +33 checks (20-row rejection table, base+axis shapes, 256 cap,
  legacy hold shape, overlap OR, relative time, later-wins, trigger,
  start/end times, blank skipping, wait refusal).
- LIMITATION unchanged: still not compiled here; Windows build + ctest required.

## 2026-11-26 — C++ increment 18 (GUI shows what the adapter text selects)

- `graphics/adapter_select`: new portable `describeSelector()` (one-line
  meaning of a parsed Auto/Warp/Index/Name selector; names the rule only —
  resolution against real adapters happens at device creation with honest
  capability skips). No Qt, never throws, unit-tested.
- `src/cpp/src/ui/ui_shell`: Graphics Settings page gains a live
  "Selects:" hint under the adapter field (`describeSelector` of the
  current text, refreshed on every keystroke plus load/save/defaults via
  the existing `applySettingsToWidgets` path); adapter placeholder now
  lists the accepted forms. No other page changed (one behavior per change).
- Tests: +5 checks (auto/warp/index/name wording, token number embedded).
- LIMITATION unchanged: still not compiled here; Windows build + ctest required.

## 2026-11-26 — C++ increment 17 (adapter selector from newly read upstream CODE)

Code read (via GitHub REST contents API at pinned SHAs into
scratch/code/files7 + raw7):
- XCL `crates/xcl/src/gpu/adapter.rs` @ `b49be1836844` (Selector::parse:
  trim, empty/auto, warp, plain-number index with the leading-zero rule,
  oversized-to-Name; allows; contains_numeric_token digit-boundary rule;
  select_indices with out-of-range numeric → model-token fallback) + its
  `adapter_tests.rs` (exact vectors mirrored below) — behavior implemented.
- XCL `crates/xcl/src/eh.rs` @ `b49be1836844` (sampled: doc comment + fn
  inventory — RtlAddFunctionTable, RtlPcToFileHeader hook, unhandled filter,
  GS-cookie checks, CONTEXT emulation, single-step probes) — read only,
  refused (hook/execution-coupled; crash reporting already ours).
- victory-fork `crates/victory-gpu/src/stats.rs` @ `ceb9e1f7a018` (Stats
  shape, NOW/NOW-once thread, system/process/thread times, PDH engine
  counters, parse_instance + busiest with exact vectors, 1 s cadence, 5-look
  log) — read only; pure parse/aggregate parts noted, OS collection
  refused.
- victory-fork `crates/victory-gpu/src/framelog.rs` @ `ceb9e1f7a018`
  (never-reset totals, per-frame diffs, median/long_over/max(usual*1.5,
  usual+40 ms)/RECENT=64/LONG_MOST=8 report shape, exact long-frame test
  vectors) — read only; reporting shape noted, present/submission/card/
  slow-core machinery refused (title/GPU-coupled).
- victory-fork `apps/victory/src/translate.rs` @ `ceb9e1f7a018` (partial:
  Tally fail/merge with reason → (count, first example), --warm hash walk
  with time limit, run batch shape) — behavior only; batch tally noted for
  our shader tools, victory-shader specifics refused (title-coupled).
- Xbox2Windows `src/startup_stubs/local_log.h` @ `8c9f168f4746` (env_on
  last-error preservation, capped stderr logf, first_calls saturating gate,
  probe_readable/accessible foreign-memory probing) — read only; probing
  refused (never touch foreign memory), gate idea already covered by
  FirstSeenThrottle.
- Xbox2Windows `src/startup_stubs/resource_budget.h` @ `8c9f168f4746`
  (live/retained per-category byte/object snapshot, Ticket on private data)
  — read only, refused (D3D12 Ticket/COM/SRWLOCK-coupled).
- Xbox2Windows `src/startup_stubs/input_script.h` @ `8c9f168f4746`
  (comma-separated script → 256 capped entries with rejections, parse_when/
  wait/mark/group grammar, play(t) button-OR + later-axis-wins, Timeline
  picture-signal waits) — read only; parse/play core noted for a Controls
  scripted-input increment, signal waits refused (presenter-coupled).
- No code copied. All behavior below is independent implementation under MIT.

Implemented:
- `graphics/adapter_select`: AdapterSelector Auto/Warp/Index/Name +
  `parseAdapterSelector` (trim, auto/warp words, "0"-but-not-"007" index,
  overflow-to-Name), `containsNumericToken` (digit-boundary match),
  `selectAdapterIndices` (Auto → hardware in order, Warp → software,
  Index → n-th hardware else numeric-token fallback, Name → substring over
  all adapters). `chooseAdapter` reimplemented on top with identical
  results on every historical case (empty → first hardware else 0,
  miss → WARP fallback). Portable, unit-tested.
- Tests: +26 checks (9 parse, 6 token, 7 indices, 4 choose integration).
- LIMITATION unchanged: still not compiled here; Windows build + ctest required.

## 2026-11-26 — C++ increment 16 (Benchmark shows the frame-rate box, honestly)

- `src/cpp/src/ui/backend_status`: new portable `smokeRateLine(drawMs)`
  (one-shot smoke draw time → `frameRateMsText` via `tenthsFromFps`, always
  labelled "single smoke sample — not a sustained stream"; zero/negative/
  NaN/inf draw times report "no frame-rate sample" instead of a faked rate)
  + `smokeBottleneckLine()` ("bottleneck: unknown", one-shot smoke has no
  continuous stream). No Qt, unit-tested.
- `src/cpp/src/ui/ui_shell`: Hardware Benchmark page gains a second readout
  (`benchRate_`) showing the rate + bottleneck lines on success and an
  explicit no-sample note on every SKIP/FAILED path; success trace now carries
  the rate line. No other page changed (one behavior per change).
- Tests: +10 checks (10 FPS / 5.8 FPS / 1000 FPS / clamp vectors, zero /
  negative / NaN / inf no-sample, honest wording, bottleneck line).
- LIMITATION unchanged: still not compiled here; Windows build + ctest required.

## 2026-11-26 — C++ increment 15 (strict CLI parsing from newly read upstream CODE)

Code read (via GitHub REST contents API at pinned SHAs into
scratch/code/files6 + raw6; raw.githubusercontent blocked as before):
- XCL `crates/xcl/src/config.rs` @ `b49be1836844` (per-title config +
  load-or-create that never replaces an unreadable file, old-default
  migration that keeps user values, `resolve()` manifest budgets, trim +
  alias canonicalization, `{game}`/`{data}` expansion, saturating quotas;
  exact unit-test vectors) — behavior only; placeholders/canonicalization
  queued for the settings layer.
- victory-fork `apps/victory/src/run.rs` @ `ceb9e1f7a018` (prepare → bind →
  report → gate on `--execute`, crash reporting, loader lock, LateDlls
  attach/detach discipline) — read only; execution refused, loader-lock
  threading refused (our run-fixture stays census/report-only).
- victory-fork `apps/victory/src/shader.rs` @ `ceb9e1f7a018` (2 GiB skip cap,
  magic scan, per-file + total counters, sorted top-10, "none found" note) —
  read only; our 256 MiB intake cap already covers the bounded-intake idea.
- Xbox2Windows `export_write_mask.h` @ `8c9f168f4746` (exact-shape manifest
  lines, duplicate-target rejection, shared-state expansion) — strictness
  pattern applied to the CLI below.
- Xbox2Windows `command_signature_filter.h` @ `8c9f168f4746` (pre-call scan
  for the unsupported case, no Windows dependency) — read only, no consumer.
- Xbox2Windows `console_mode.h` @ `8c9f168f4746` (pure registry-answer
  helpers: subkey match, answer_for, fill_query/fill_get) — read only,
  refused (no registry virtualization in this tree).
- Xbox2Windows `file_lock.h` @ `8c9f168f4746` (bounded cross-process lock,
  backoff, busy-code mapping, overflow-safe deadline, RAII release) — read
  only; queued (needs a caller; same-process contention tests are
  platform-risky).
- Xbox2Windows `draw_dump_tracking.h` @ `8c9f168f4746` (GPU readback ring) —
  read only, refused (D3D12-coupled).
- Plus the already-read XCL `cli.rs` strict-parser behavior (unknown/stray/
  missing/malformed rejected) applied here.
- No code copied. All behavior below is independent implementation under MIT.

Implemented:
- `app/cli_parse`: new `sb_cli` lib — `Parser` (nextArg/atEnd, value(),
  parseU64/parseBool01/parseHexU64 with exact error strings, unexpected(),
  1-16-digit parseHexU64Str). Portable, unit-tested.
- `scarlettbridge-cli`: every subcommand now parses strictly (positionals
  required, flags allowlisted per subcommand, extras rejected as JSON
  `{"ok":false,"error":...}` + usage, exit 2 instead of silent ignore).
- `scarlettbridge-runtime`: `--config`/`--pipe` missing values and unknown
  options now fail fast (exit 2) instead of being silently skipped.
- Tests: +19 checks (missing/flag-as-value/malformed/negative/u64,
  bool01 range + 0/1, hex malformed/0x/bare/empty/too-long/caps, three
  unexpected() wordings).
- LIMITATION unchanged: still not compiled here; Windows build + ctest required.

## 2026-11-26 — C++ increment 14 (Shader Manager shows real cache stats)

- `src/cpp/src/ui/backend_status`: new `queryCacheStats()` (read-only
  `ShaderCache::stats` entry/byte count vs entry/MB budgets; a missing dir
  reads as zero, never throws) + `cacheStatsLine()` (one-line summary with
  within-budget / BUDGET-EXHAUSTED verdict) + `kDefaultCacheMaxEntries`
  (4096, mirrors the ShaderCache default). Portable, no Qt, unit-tested.
- `src/cpp/src/ui/ui_shell`: Shader Manager page gains a real read-only
  stats row + Refresh button driven by `scarlettbridge-settings.json`
  (falls back to defaults when the file is missing/corrupt, same rule as
  the graphics page; "disabled in settings" is reported honestly instead
  of scanning). Stats refreshes are counted + traced. No other page changed
  (one behavior per change).
- Tests: +8 checks (entry/byte counts ignoring non-.bin, budget-ok line,
  byte/entry over-budget + exhausted line, singular "1 entry", missing dir).
- LIMITATION unchanged: still not compiled here; Windows build + ctest required.

## 2026-11-26 — C++ increment 13 (frame-rate summary box from newly read upstream CODE)

Code read (via GitHub REST contents API at pinned SHAs into
scratch/code/files5 + raw5; raw.githubusercontent blocked as before):
- victory-fork `crates/victory-gpu/src/fpsbox.rs` @ `ceb9e1f7a018` (six-line
  box: rate+ms, CPU/game cores, top thread, GPU/game share, VRAM used/budget,
  RAM used/free; tenths `"{}.{} FPS"` + `"{:.0} MS"` = 10000/tenths;
  cut-or-pad `fit`; Off/Rate/All mode switch; bottleneck comment —
  thread~100% + idle card = thread-held, card~100% = self-held, neither =
  waiting; unit tests with exact vectors) — behavior only.
- victory-fork `crates/victory-gpu/src/drvwatch.rs` @ `ceb9e1f7a018`
  (trip-latch: first driver error trips the switch + counts all; EventLog
  subscription with vendor query; Auto HDR 4125 false-positive lesson) —
  read only; OS subscription refused, latch noted for later.
- victory-fork `crates/victory-core/src/config.rs` @ `ceb9e1f7a018`
  (resolution env > file > default, empty env ignored, explicit-missing
  config errors, always-outside-repo cache/data subdirs, split list env) —
  read only; env-resolution helper noted for later (repo guard already ours).
- victory-fork `crates/victory-core/src/logging.rs` @ `ceb9e1f7a018`
  (VICTORY_LOG else RUST_LOG, default level, diagnostics-gated trace cap) —
  read only; not implemented (app-layer concern).
- victory-fork `crates/victory-gpu/src/query.rs` @ `ceb9e1f7a018` (query heap
  remembers its description; nothing runs) — read only, refused (COM vtable
  territory; our Unsupported-stub pattern already covers it).
- victory-fork `apps/victory/src/inspect.rs` @ `ceb9e1f7a018` (human-readable
  PE report + import-library grouping) — read only; text report noted for
  later (our pe-census/pe-exports JSON cover the machine side).
- XCL `crates/xcl/src/cli.rs` @ `b49be1836844` (strict option parser:
  rejects unknown options, stray positionals, missing values, malformed
  numbers/hashes instead of falling back to defaults) — read only; our
  `cli_main`/`runtime_main` parsing is loose (positional checks, unknown args
  ignored), so a strict-parser helper is a queued later increment.
- XCL `crates/xcl/src/gpu/fence.rs` @ `b49be1836844` (FenceError:
  removal-before-completion incl. UINT64_MAX sentinel, re-sample on wake,
  timeout-vs-removal, mock-vtable tests) — read only, refused (D3D12
  fence/event handles; removal reporting already ours via
  deviceRemovedReason).
- Xbox2Windows `src/startup_stubs/api_trace.h` @ `8c9f168f4746`
  (declared-output compare contract, Outcome Unknown/Unreadable/Same/Changed
  + partial, 64-byte bounded snapshot, `changed:0,1,partial` format; never
  probe arguments as pointers) — read only; compare helper noted for later.
- Xbox2Windows `tools/call_evidence.py` @ `8c9f168f4746` (dumpbin+pefile
  call-site inventory, DXBC container scan, sibling-temp atomic JSON write) —
  read only, refused (dumpbin/pefile tool-coupled, title-DLL-specific; import
  inventory already ours via pe-census).
- No code copied. All behavior below is independent implementation under MIT.

Implemented:
- `telemetry/frame_box`: FrameBoxMode Off/Rate/All + `parseFrameBoxMode`
  ("0"→Off, "fps"→Rate, else All; env reading stays the app's job),
  `tenthsFromFps` (nullopt on negative/NaN/inf, round + clamp 99999),
  `frameRateText`/`frameRateMsText` (tenths 0 → rate alone),
  `fitBoxLine` (cut-or-space-pad; ASCII-only output), `frameBoxLines`
  (six fixed-width 26-glyph lines, blank-when-missing), `classifyBottleneck`
  (>= 90 = near-100 %; saturated card first, then hot thread, else
  Unknown-when-blind / Waiting) + `bottleneckName`. No OS, no GPU.
- Tests: +49 checks (modes, rate/ms vectors incl. 9999.9/0 MS and 5.8/172 MS,
  tenths edge cases, fit pad/cut/exact, full-box exact lines + widths,
  unknown-card + all-blank variants, five bottleneck paths + names).
- LIMITATION unchanged: still not compiled here; Windows build + ctest required.

## 2026-11-26 — C++ increment 12 (GUI surfaces the refusal census)

- `src/cpp/src/ui/backend_status`: new `nullCensusLine()` (total + per-cause
  entry/buffer/token/wrapper/dimension, sums by construction),
  `perReasonCensusLine()` (total + distinct keys/pairs + overflow), and
  `perReasonDetailLines()` (summary + up to N detail lines via
  `report(false)`, so repeated views never consume delta state). Portable,
  no Qt, never throws. Unit-tested.
- `src/cpp/src/ui/ui_shell`: the shell now owns a real `NullCensus` +
  `PerReasonCensus` and `refreshDiagnostics()` shows them at the top of the
  Diagnostics view (filter applies to detail lines; an honest "no translated
  draws refused yet" note when both totals are zero — the synthetic ticker
  binds no descriptors). Placeholder text updated. No other page changed
  (one behavior per change).
- Tests: +10 checks (null zero/counts incl. ignored-None, reason zero/counts,
  first-note detail budget, repeat quiet, sanitize, cap, overflow line).
- LIMITATION unchanged: still not compiled here; Windows build + ctest required.

## 2026-11-26 — C++ increment 11 (refusal census from newly read upstream CODE)

Code read (via GitHub REST contents API + two new fetches; raw.githubusercontent blocked):
- Xbox2Windows `docs/startup-runtime.md` (full: null-binding causes entry/buffer/token/wrapper/dimension summing to the total; per-reason refusal census: 128 distinct pairs, 64-byte sanitized reasons, delta reports, overflow counts; plus frame-copy refresh, root-constant drop/truncate, first-8/first-4 log budgets — behavior only).
- Xbox2Windows `src/startup_stubs/null_binding_cause.h` + `per_reason_refusal_census.h` (fetched new via contents API into scratch/code/files4 + raw4: BindingFacts decision order, 5 names, 128/8/32/64 caps, sanitize fallback "unknown", linear scan, overflow + delta report shape — behavior only).
- five-star `docs/pipeline-cache.md` (full: 0x80 header, 0x70 graphics / 0x18 compute records, metadata/shader indexes, semantic-matched input layout, 0x41/0x42 bindings — read only, NOT implemented; too title-data-coupled for this tree).
- five-star `ImportClassification.cs` (Kind+Detail record — already covered by our ImportClass verdicts; no change).
- victory-fork `crates/victory-gpu/src/host.rs` (sampled: pc_state mask 0x1ffffff, buffer-only stripping, VRAM/RAM idle-timeout scaling, retire/reaper/trail/removal-context — read only, NOT implemented; driver-coupled).
- victory-fork `crates/victory-gpu/src/pipeline.rs` (full: content-hash shader dedup, translate-once-on-first-use, builder all-or-none, drop counting sorted most-first — read only, NOT implemented; execution-coupled).
- No code copied. All behavior below is independent implementation under MIT.

Implemented:
- `telemetry/refusal_census`: NullCause (Entry/Buffer/Token/Wrapper/Dimension + None), BindingFacts + `classifyNullBinding` (bound-not-null, entry → token → texture chain → buffer order), `nullCauseName`, NullCensus (per-cause counts, total by construction), PerReasonCensus (128-entry cap, 64-byte sanitized reasons with CR/LF→space + "unknown" fallback, 32-byte label, first-8 detail budget, distinct-key counting, total/overflow, delta reports with hex keys).
- Tests: +~20 checks (8 classify paths, census sum + ignored-None + names, detail budget/spent, sanitize empty/newline/cap, full/delta-quiet/delta-grows reports, 128-cap + overflow counts).
- LIMITATION unchanged: still not compiled here; Windows build + ctest required.

## 2026-11-26 — C++ increment 10 (filterable diagnostics/logs + guarded export)

- `src/cpp/src/ui/backend_status`: new `redactLine()`, case-insensitive
  `matchesFilter()`, versioned `diagnosticsJson()` (counters + redacted trace),
  and `exportTextOutsideRepo()` (refuses empty/in-tree paths via
  `ensureOutsideRepo`, creates parents, never throws). Unit-tested.
- `src/cpp/src/ui/ui_shell`: Diagnostics page gains a filter box and
  redacted-JSON export (filtered trace + counters, `diagnostics.exports`
  counter); Logs page is now a real view (capped 2000-line source,
  filter box, redacted-text export, view refresh). `logLine()` feeds the
  filter source + `LastOutputRing`; `refreshDiagnostics()`/`refreshLogs()`
  apply the filter (last 50 trace / 500 log lines shown).
- Tests: +8 checks (filter empty/CI/no-match, drive-path redaction,
  JSON shape + redacted msg + counter, in-tree/empty refusal, outside write
  + roundtrip).
- LIMITATION unchanged: still not compiled here; Windows build + ctest required.

## 2026-11-26 — C++ increment 9 (full schema-v3 graphics settings editor)

- `src/cpp/src/ui/backend_status`: new option lists + validators mirroring
  `core/config.cpp` allowlists (`settingBackends/WindowModes/Resolutions`,
  `isValid*`) plus deterministic `settingsSummary()`. Unit-tested.
- `src/cpp/src/ui/ui_shell`: Graphics Settings is now a full schema-v3
  editor — adapter, backend (locked to tested d3d12), window mode, resolution,
  vsync, frame cap, texture budget, shader-cache toggle, cache MB, cache path,
  precompile, debug layer, DRED — with Reload / Save / Defaults. Load and save
  both round-trip through `fromJson(toJson())` so corrections surface as
  warnings (never silently kept); saves are counted in diagnostics. No other
  page changed (one behavior per change).
- Tests: +6 checks (option lists, validators, summary content, validator
  correction for an illegal resolution).
- LIMITATION unchanged: still not compiled here; Windows build + ctest required.

## 2026-11-26 — C++ increment 8 (Qt shell wired to real portable backends)

- New `src/cpp/src/ui/backend_status.{h,cpp}` (portable, no Qt): `versions()`
  from `core/version.h`, `scanFolderReadOnly()` (read-only, caps 500 entries /
  40 hashed / 5 MiB / 64 KiB first chunk, redact option, never writes source
  dir — same contract as `cli validate-folder`), one-line `systemLine()`,
  `gamepadLine()`, `audioLine()`, `adapterLine()` (capability skips, never
  faked), `cacheFingerprint()`. New `sb_ui_support` static lib; `sb_tests` and
  `scarlettbridge-ui` link it (`scarlettbridge-ui` now links the full runtime
  set instead of only core+ipc).
- `ui_shell.{h,cpp}` rewritten around real backends: dashboard shows the
  in-process `Lifecycle` + system/versions; top bar gains synthetic Crash +
  LIVE/DEMO label; setup page scans with Browse + redact; shader page shows
  the real translator fingerprint with CLI pointers; diagnostics shows the
  real `Counters` + `TraceRing` fed by the ticker and live frames; controls
  enumerates gamepads/audio with refresh; benchmark runs the in-process D3D12
  offscreen smoke with honest SKIP wording (exit-77 equivalent) and points to
  `sb_gpu_probe`; about shows real versions. Synthetic crash emits a redacted
  `CrashReport` JSON to the log and keeps the UI alive.
- Tests: +9 checks (versions, fingerprint, all four summary lines non-empty,
  scan-missing fails, temp-dir scan ok + redact).
- LIMITATION unchanged: still not compiled here; Windows build + ctest required.

## 2026-11-26 — C++ increment 7 (route table + init order + bindings)

Code read (via GitHub REST contents API; raw.githubusercontent blocked):
- XCL `hle/mod.rs` full (Registry build: user aliases first, kernel/xgameruntime/d3d12/audio/media/user/storage/texture/controller/compiler routes, glob patterns, Route::Hle/System/Stub, route/hle_for/find_any, aliases_are_system via "@self", default_routes/aliases-precedence/cross-module tests).
- victory-fork `registry.rs` (serves() trailing-'*' prefix, hle_export! naming rule, export uniqueness test, api-set prefix test) + `hle/lib.rs` (three-kind DLL policy table, modules→host→Missing order, HleResolver classify, console-vs-shared tests) + `modules/mod.rs` (per-area modules, census-driven planning) + `loader/resolve.rs` (Provenance Hle/Host/Title, Missing stub rule, case-insensitive compare note).
- ReCity `main.cpp` (dry-run/init/run modes, exactly-one-mode guard, per-module binding counts + HRESULT return-policy report, ExitProcess semantics — execution refused, reporting only).
- No code copied. All behavior below is independent implementation under MIT.

Implemented:
- `runtime/routes`: RouteKind Hle/System/Stub, case-insensitive glob (`*` any run), RouteTable (user aliases front-insert = precedence, canonical defaults, hleFor, cross-module findAny ordinal-safe, per-module export uniqueness), `defaultRoutes()` (kernel/xgameruntime/d3d12/audio/media/user/storage/texture/controller/compiler families).
- `runtime/init_order`: Binding Shim/Host/Module/Unimplemented + `bindingFromSource`, `BindingCounts`, `computeInitOrder` (deps-first DFS, host/shim imports ignored, case-insensitive, cycles broken at closer, every module exactly once), `parseRunMode` (exactly-one guard).
- Tests: +~30 checks (canonical defaults, alias precedence/system/stub/HLE, glob, find-any, ordinal refusal, duplicate rejection, deps-first/case/cycle, run-mode guard, binding counts).
- LIMITATION unchanged: still not compiled here; Windows build + ctest required.

## 2026-11-26 — C++ increment 6 (exports + forwarders + image layout)

Code read (via GitHub REST contents API; raw.githubusercontent blocked):
- XCL `loader.rs` map-image flow (reserve+commit, section copy bounds, DIR64/HIGHLOW apply, import resolve + delay module-handle write, TLS install bounds + callback list, NO code run, page-protection pass, exception-table + cookie hooks, load-game-dll idempotence).
- victory-fork `victory-pe/src/image.rs` exports/TLS/range remainder (Address(0) gap skip, ForwardByName/ForwardByOrdinal "dll.name"/"dll.#ord", va_to_rva, MAX_TLS_CALLBACKS 1024, rva_to_file_offset 4 GiB overflow guard, own-binary + garbage tests).
- Xbox2Windows `pe_map.cpp` (MapStatus codes, inside(), Image::at/string, protection(), executable()/writable_section() spans + TLS-index writability rule, load-config cookie seed bounds) + `import_routing.py` (POLICY_VERSION 2, ROUTE_KIND/RESOLUTION constants, hle/stub/windows routes + api prefixes, classify/annotate without resolving) + `compat_report.py` (_symbol_key ordinals-first, _row_key deterministic sort, SCHEMA/TOOL_VERSION, TLS/SEH/load-config features) + `pe_inventory.py` (KNOWN DLL inventory notes — behavior only).
- ReCity `pe_loader.h/.cpp` (Binding Shim/Host/Module/Unimplemented, ImportRecord "#ord", Module exports/forwarders maps, require_range/read_string/rva_in checks, TLS host slot rules — behavior only).
- No code copied. All behavior below is independent implementation under MIT.

Implemented:
- `formats/pe_exports`: export-directory census from bytes (Base + function/name/ordinal tables, Address(0) gaps skipped, in-directory RVAs decoded as "dll.symbol"/"dll.#ord" forwarders, caps 65536/256; corrupt rows skip, absurd tables fail). Plus `ImageFeatures` (TLS/exception/load-config presence, SEH entries/trailing) and `exportLess` (ordinals-first deterministic sort).
- `runtime/forwarder`: `parseForwarderSpec` (LAST-dot split for dotted DLLs, "#n" ordinals, canonical ".dll", nullopt on bad spec) + `resolveForwarderChain` (depth cap 8, missing → nullopt never a stub).
- `runtime/image_layout`: `sectionProtection` (X/R/W mapping), `rvaInsideImage` (overflow-safe), `sectionSpan` (vsize else raw), `isExecutableRva`/`isWritableRva` (by characteristic bits + span), `isValidEntryPoint` (nonzero + inside + executable), DIR64/HIGHLOW `applyReloc` (others warn-false), `validateTlsCallbacks` (inside-image, cap 1024).
- `formats/pe_imports`: `PeSection` gains `characteristics` (parsed at +36; existing rows unaffected).
- `cli pe-exports <pe>`: deterministic sorted JSON (exports/named/forwarded counts, features, rows with nulls), MZ prefilter + 256 MiB cap. Never loads or executes.
- Tests: +~40 checks (synthetic export dir: named + forwarder + ordinal-only + gap-skip + sort + machine/truncation rejects; forwarder parse/chain/missing/loop-cap; layout protections/containment/relocs/TLS).
- LIMITATION unchanged: still not compiled here; Windows build + ctest required.

## 2026-11-26 — C++ increment 5 (resolver log, trace throttle, cache key v2, host-path guard)

Code read (via GitHub REST contents API; raw.githubusercontent blocked):
- XCL `resolver.rs` remainder (classify dry-run without mapping, ImportRecord log, TraceFilter wants, GetProcAddress-null contract, LoadLibrary kinds, mapped-forwarder depth/dotted-DLL rules — behavior only).
- victory-fork `runtime.rs` elements() + Title::from_manifest vectors + use_console_timers (1 ms + efficiency opt-out), `host.rs` default allow-list + System32-only resolve, `no_dll_planting.rs` test.
- victory-gpu `shader_cache.rs` build()/warm()/cached() two-attempt retry + counts.
- AnyXboxX `xgameruntime.cpp` FirstQuery (128-entry pair table, table-full silent) + System32 full-path delegation.
- five-star `GdkThunkScanner.cs` (call+LEA/busiest-target heuristic — read only, NOT implemented; too execution-coupled for this tree).
- No code copied. All behavior below is independent implementation under MIT.

Implemented:
- `runtime/resolve`: `ResolvedSource` Hle/HleOther/GameDll/System/Stub, `ImportRecord` log, `ImportResolver` (HLE exact → game DLL → system policy+export → HLE-other named-only → Stub; ordinal `#n` never cross-DLL; `getProcAddress` null-on-missing never stub; `loadLibrary` hle/gamedll/system/null). `ImportRegistry` gains `hasAnyHle` + `findHleProvider` (ordinal-safe).
- `telemetry/trace_filter`: `FirstSeenThrottle` (cap 128, table-full silent) + `TraceFilter::parse/wants` (hle/all/include/exclude, stub-never).
- `runtime/host_path`: `system32PathFor` (drops directories), `isSystem32Path`, `isPlantingAttempt`.
- `runtime/console_timer`: 1 ms policy, `clampTimerRequestMs` 1..16, `efficiencyOptOut` (exact "1" keeps system choice).
- `graphics/pipeline_cache`: `translatorFingerprint` + `cacheKeyV2` (sbpc2) + `getOrTranslate` two-attempt warm with hit/miss counters.
- Tests: +~30 checks (resolver order/records/contracts, host-path planting, timer policy, throttle/filter, cache v2 miss→hit + corrupt retry via temp dir).
- LIMITATION unchanged: still not compiled here; Windows build + ctest required.

## 2026-11-26 — C++ increment 4 (more upstream CODE read, synthetic PE census)

Code read (via GitHub REST contents API; raw.githubusercontent blocked):
- five-star-recomp: `PeImage.cs` (DOS/COFF/PE32+ layout, directory table, RVA→offset via max(vsize,raw), headers fallback, NUL-terminated Latin1), `ImportClassifier.cs` (shim → fallback-stub → package/manual/host/missing; report reads the loader's own verdict), `FaultReporter.cs` (vectored handler first+last, ContinueSearch/Execution, severity + customer-bit test, mapped-frame depth 12, full-3 / max-64 / sites-1024, thread reentrancy guard, new-site-despite-quota), `GdkThunkCode.cs` (LEA/modRM byte forms), `ImportOrdinalNames.cs`, `PeImport.cs`.
- victory-fork: `victory-pe/src/image.rs` (single-pass ImageInfo, ImportModule case-preserved dll, ImportSymbol Name{hint,name}/Ordinal with `#n` display, TLS/reloc/exception/load-config/codeview fields), `victory-gpu/src/shader_cache.rs` (translator-fingerprint + own-source-bytes cache key, content-addressed atomic entries, offline warm), `victory-loader/src/{resolve,lib}.rs` (ImportResolver trait shape), `victory-gpu/src/pipeline.rs` (sampled).
- XCL: `resolver.rs` (Source Hle/HleOther/System/GameDll/Stub + ImportRecord log, cached system modules, case-insensitive game-DLL search, HLE→game-folder→system→stub order), `loader.rs` (map-but-never-run, reserve+commit, DIR64/HIGHLOW relocs, section protections), `hle/d3d12x/mod.rs`, `hle/kernelx/lazy.rs` (sampled), `data/stubdb.toml` (module E_NOTIMPL defaults, `*!` function overrides, per-DLL ordinal maps — facts noted, table NOT copied).
- Xbox2Windows: `src/d3d12_xs_shim.cpp` (observed console flag bits, static_assert layout checks, permissive-retry wrapper provenance by object), `src/era_gpu_alias.*` (sampled).
- AnyXboxX `runtime/gpu_runner/adapter.cpp` (env filter, hardware-only order, case-insensitive substring match, WARP fallback, no-match listing with available adapters, vendor/device/driver/lane/raytracing/double-precision describe).
- ProjectReCity `runtime/src/main.cpp` (--dry-run/--init/--run modes, exactly-one-mode, binding counts shim/host/module/unimplemented + HRESULT return policy, ExitProcess semantics).
- No code copied. All behavior below is independent implementation under MIT.

Implemented:
- `formats/pe_imports`: minimal AMD64 PE import census from bytes (MZ/e_lfanew/PE-sig/COFF/PE32+ dirs/sections, RVA→offset, 20-byte import + 32-byte delay descriptors with RVA-vs-VA bias, ordinal top-bit, hint+name, caps 96/4096/65536/256). Structural failures fail; corrupt rows skip. Never loads or executes.
- `cli pe-census <pe> [--allow-system]`: deterministic sorted JSON rows with `censusDll` class + honesty fields and per-class summary; MZ prefilter + 256 MiB cap.
- `runtime/title`: `TitleManifest::parse` (BOM/comment/CDATA/entity-tolerant element scan, hex TitleId, StoreId, SizeMB quota, saves-beside-storage) + `ImageRegistry` (main base, base→path, case-insensitive `imageNamed`, forget).
- `telemetry/fault_filter`: `FaultBudget` (customer-bit/breakpoint/severity test, full-3/max-64/sites-1024 quotas, new-site-despite-quota, thread reentrancy guard).
- `graphics/adapter_select`: portable `chooseAdapter` (hardware-first, warp request, case-insensitive substring, WARP fallback) + `isWarpRequest`.
- Tests: +~25 checks (synthetic PE with normal/ordinal/RVA-delay/VA-delay rows + machine/magic/truncation rejects, manifest vectors, registry, budget quotas, chooser, HLE table semantics).
- LIMITATION unchanged: still not compiled here; Windows build + ctest required.

## 2026-11-26 — C++ increment 3 (from reading upstream CODE, not just docs)

Code read (via GitHub REST contents API; raw.githubusercontent blocked):
- victory-fork: `crates/victory-core/src/policy.rs` (ensure_outside_repo + marker + longest-prefix canonicalization), `crates/victory-hle/src/{host,registry,lib,runtime}.rs` (HostPolicy allow patterns, registry serves/lookup, HleResolver order modules→host→Missing, main-image + title-manifest runtime state, console timers), `apps/victory/src/census.rs` (MZ prefilter + per-dll/per-function census without running), `tests/fixtures/hle-probe` (raw-dylib imports, title DLL, TLS callbacks, DllMain counts).
- XCL: `crates/xcl/src/{host,stubs}.rs` + `hle/{mod,kernelx,xgameruntime}` (routing tables + aliases + fallbacks, StubDb module-defaults/functions/ordinals + call counting + first-caller, host boot/activity/memory logging).
- Xbox2Windows: `tools/{import_routing,pe_inventory,compat_report,intake_exe}.py` (candidate routes + honesty fields, MZ/API-set/KnownDLLs inventory, deterministic JSON+MD, loader-verdict agreement), `docs/non-windows-hosts.md` (platform split + exit-77), `src/adapter_identity.*` (D3DMT open/query/PnP-key validation).
- five-star-recomp: `PeImportReader.cs` (bounded 4096-descriptor/65536-thunk loops, ordinal top-bit, delay RVA-vs-VA bias), `GdkThunkScanner.cs` (call+LEA patterns, busiest-target heuristic), `docs/exe-intake.md` (report-not-exploration, loader-owned classification, Host/Missing are machine facts).
- AnyXboxX `runtime/xgameruntime/xgameruntime.cpp` (System32 full-path delegation, first-query-only bounded tracing, QueryApiImpl E_NOINTERFACE default), ProjectReCity `runtime/src/pe_loader.h` (Binding Shim/Host/Module/Unimplemented, TLS index rules, init-after-deps order).
- No code copied. All behavior below is independent implementation under MIT.

Implemented:
- `imports`: HostPolicy prefix-`*` + case-insensitive matching; `registerHle`/`hasHle` coverage table; new `censusDll(dll,symbol)` with order HLE→host-policy→Missing/StubTrap + `routeKind="candidate"`, `resolution="not_checked"` honesty fields; ordinal `"#n"` symbols supported. Existing `census()` (SBM path) kept compatible.
- `runtime/stubs`: new `StubDatabase` (module defaults, exact + `*!name` wildcard, fallback-dll lookup, ordinal names, counted calls + first-caller, mutex-guarded, case-insensitive keys). No executable thunks emitted — resolve() still never fakes success.
- `telemetry`: `LastOutputRing` (bounded 8-line title-output context), `formatFaultReportWithBase` (module+RVA), `writeCrashLog` (unix time + pid, creates dirs, refuses in-tree paths).
- `core/filesystem_guards`: `ensureOutsideRepo()` (longest-existing-prefix canonicalization + `SB-ROOT-MARKER.txt` ancestor scan). Wired into `ShaderCache::store`, `sb_gpu_probe --out`, `writeCrashLog`.
- `cli run-fixture --census` rows now carry `route_kind`/`resolution`/`provider`.
- Tests: +~20 checks (patterns, DLL census + ordinals + honesty, stub precedence/counts, ring bounds, module+RVA, crash-log write, in-tree refusal incl. non-existent tail).
- LIMITATION unchanged: still not compiled here; Windows build + ctest required.

## 2026-11-26 — C++ increment 2 (from re-reading fetched repos)

Sources re-read: victory-fork `docs/architecture.md` (import resolution order,
HostPolicy idea, census-without-running, module+RVA crash naming, shader-at-
pipeline-creation + cache); XCL `THIRD_PARTY.md` (per-file origin+date header
annotations, DXC-from-user-SDK-not-distributed, system-library exception,
GPL-2.0-only compatibility list, VIctory-under-GPL-2.0-with-author-permission
precedent 2026-11-24); Xbox2Windows `AGENTS.md` (title-neutral wording, source-
built fixtures only, out/ + game-files/ gitignored, never weaken tests, one
PR = one behavior; `_warp` vs `_hardware` suffixes; skip-77 separate from
pass/fail; GPU reports identify adapter/driver/Windows/commit/debug-layer;
evidence list: commit, command, observed vs expected, environment, first
mismatch, skips, negative control).

Implemented (independent code, no copying):
- `imports`: HostPolicy default-deny allowlist, StubMode Trap/LogAndFail
  (explicitly NO zero-return-success mode — divergence from the fork's `zero`
  mode documented in FEATURE_SELECTION), `census()` classification incl.
  host-shaped (.dll) names; `run-fixture --census` report.
- `ipc`: `PipeServer::acceptWithTimeout` (overlapped, cancellable, returns to
  listen state); runtime_main publishes state+FrameStats to `--pipe` with
  stdio fallback and zero extra threads (pollAccept in run loop).
- `telemetry`: `formatFaultReport` (module+RVA-style detail); runtime SEH
  filter emits redacted CrashReport JSON on unhandled exceptions.
- `ui`: `IpcClient` QThread worker (validated frames only, malformed skipped),
  Connect-live button + LIVE/DEMO mode label with synthetic fallback.
- `graphics`: timestamp query heap in smoke test (ticks + frequency in
  report), `deviceRemovedReason()` surfaced in probe JSON.
- Tools: `gpu_probe`/`shader_probe`/`cli translate` return exit 77
  (capability skip) when D3D12/validation is unavailable; probe JSON now
  carries commit, debugLayer, windowsBuild, adapter/driver/VRAM, timestamps.
- Tests: +14 checks (policy, census, stub modes, redaction, crash format,
  SBM roundtrip, ALU reference pixel (1.5,0.75,0.6,1.5), lifecycle history,
  IPC stream framing, config save/load, cache budget refuse).
- LIMITATION unchanged: still not compiled here; Windows build + ctest
  required before further C++ work.

## 2026-11-26 — C++ tree skeleton (src/cpp), independent implementation

- Added `src/cpp/`: CMake root + dev/release/ci presets + vcpkg manifest (zero
  third-party deps by design), MIT LICENSE with provenance note,
  `config/default-settings.json` (schema v3), `THIRD_PARTY_NOTICES.md`.
- Core (portable, no deps): result types, strict JSON parser/serializer,
  FNV-1a + SHA-256, structured logging + redaction, settings load/save/migrate.
- IPC v1: same message families/versioning/caps as the web prototype, length-
  prefixed framing, Windows named-pipe transport with fail-closed owner-only ACL.
- Runtime: lifecycle state machine with transition table, SBM1 synthetic-module
  loader (bounded reader, 64 MiB caps), import registry (unresolved = fatal),
  read-only platform services with approved-root containment check.
- Graphics: backend abstraction; D3D12 adapter enum + debug-layer device +
  offscreen smoke triangle + pixel readback (swapchain/present = Unsupported);
  SBVS shader IR + verifier + CPU interpreter + HLSL emitter + D3DCompile
  validation (DXC loader = Unsupported with reason, Phase 7); versioned
  pipeline cache with corruption-discard + budgets.
- Media/platform/telemetry: XInput presence check, WASAPI enumeration note,
  system report, counters/trace-ring/redacted crash reports.
- Apps: `scarlettbridge-runtime` (staged lifecycle + JSON-lines events),
  `scarlettbridge-cli` (version/probe/validate-folder/run-fixture/translate),
  Qt Widgets shell (dashboard/logs/settings real; other pages honest pending).
- Tools: `sb_gpu_probe`, `sb_shader_probe`, `sb_fixture_builder`,
  `sb_cache_inspector`. Tests: `sb_tests` CPU-only suite (~40 assertions).
- Provenance: no upstream source copied; SOURCE_PROVENANCE stays empty of
  reused rows. New files are original work under MIT.
- LIMITATION: this tree has NOT been compiled — no MSVC/Windows SDK in this
  workspace. First Windows build must run `cmake --preset dev` + `ctest` and
  record the result in STATUS_EVIDENCE before any further C++ work.

## 2026-11-26 — Live audit of all brief links (Phase 0 gate, part 1)

- Fetched all 11 repo links + 3 tracker links live. 10/10 repos reachable (all pushed within ~24h); `JordanFromIT/CGBJ-VIctory` confirmed 404 and excluded.
- Recorded SHAs, dates, SPDX licenses (API + README agree), root file listings, README status quotes in REPO_AUDIT.md.
- Confirmed: 7× GPL-3.0-or-later, Xbox2Windows MIT, XCL GPL-2.0-only, ScarlettPort mixed per-part (LICENSE.md fetched: `bruntime/xcl/` GPL-2.0-only + GPL-3 tree).
- Nested-license flags: AnyXboxX THIRD_PARTY.md + submodules + MIT libHttpClient; five-star THIRD_PARTY_NOTICES.md + vendor/; XCL THIRD_PARTY.md; ReCity submodules.
- Trackers live: gtapcport (none meets its Playable stage), UFD (stages + one unlogged ~2 fps report), race (15-min checks). Reddit JSON blocked by login wall — noted, not evidence either way.
- Upstream self-reported fps/stages moved to STATUS_EVIDENCE section B, strictly labelled NOT our verification.
- Still open: full license-text pulls per file before any reuse; THIRD_PARTY content reads; submodule license checks. No upstream code copied (SOURCE_PROVENANCE still empty).

## 2026-11-26 — Web foundation (Phase 0–2 prototype)

- Added Phase 0 audit docs (initially snapshot-based, now replaced by the live audit above).
- Added web GUI shell: 9 pages, dark theme, mock/demo mode, settings persistence, simulated IPC + lifecycle, synthetic WebGL2 GPU tests, diagnostics graphs, structured logs, gamepad panel, benchmark export.
- Verified live: GPU suite 4/4, self-tests 7/7, Start/Crash/Stop lifecycle, 9-page nav, snapshots, 390px check.
- No upstream code copied. No game content. No decrypt, unpack, or key paths.
