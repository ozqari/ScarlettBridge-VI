# ScarlettBridge C++ tree (`src/cpp`)

Independent C++20 implementation of the brief's Phase 1–7 foundation. No
upstream source is vendored here (see `../docs/SOURCE_PROVENANCE.csv`); the
audited repositories were behavioral/reference material only.

## Layout

- `src/core` — result types, strict JSON, hashing (FNV-1a + SHA-256), logging,
  settings schema v3, version constants. Portable, zero third-party deps.
- `src/ipc` — versioned protocol v1 (mirrors the web prototype's `SB_IPC`)
  plus a Windows named-pipe transport with a fail-closed, current-user ACL.
- `src/runtime` — lifecycle state machine, SBM1 synthetic-module loader,
  import registry, read-only platform services.
- `src/graphics` — backend abstraction, D3D12 adapter/device/smoke-triangle
  path, SBVS shader IR + HLSL translation + compile validation, versioned
  pipeline cache, portable shader failure tally (reason → count + first
  example, merge, sorted report; no batch translator run). Unimplemented
  paths return explicit `Unsupported` errors.
- `src/formats` — bounded binary reader (all parsing goes through this).
- `src/media`, `src/platform`, `src/telemetry` — audio/input stubs with
  device enumeration where implemented, scripted gamepad input (parse/play,
  DEMO/tests), system info, metrics/trace/redaction,
  fault budgets, frame-rate summary box (fixed-width six-line box +
  bottleneck rule), refusal census (null-cause + bounded per-reason counts),
  engine-counter aggregation (instance parse + busiest-kind sums over
  caller-supplied samples; no OS collection), declared-output compare
  (Unknown/Unreadable/Same/Changed + partial; no providers yet),
  driver-error trip latch (first error trips, every error counted;
  no EventLog collection).
  Portable, unit-tested.
- `src/app` — `runtime_main` (host, strict args), `cli_main`
  (probe/validate/run-fixture, strict args), `ui_main` (Qt entry).
- `src/ui` — Qt Widgets shell: dashboard (lifecycle + system + versions),
  setup (read-only capped folder scan + redacted scan-report export), graphics settings (FULL schema-v3
  editor: adapter/backend/mode/resolution/vsync/caps/budgets/cache/flags with
  load/save/validator warnings + live adapter-selector hint), shader fingerprint + real read-only cache
  stats (entries/bytes vs budgets, refresh button) + real failure tally (shell-owned FailTally,
  summary + bounded report, honest zero note), diagnostics (refusal
  census + filterable counters/trace ring + redacted JSON export outside the
  repo + live engine-instance probe, no live figures), logs (filterable sink
  + redacted text export), controls enumeration,
  scripted DEMO input (parse + synthetic-tick playback, no device; live
  translation stays Unsupported),
  in-process benchmark with capability skips + honest single-sample
  frame-rate readout (sustained FPS never claimed), about. Synthetic source is
  always labelled DEMO; live pipe frames are LIVE.
- `src/tools` — `gpu_probe`, `shader_probe`, `fixture_builder`,
  `cache_inspector`.
- `tests/` — CPU-only unit/integration suite (`sb_tests`, CTest) + fixtures.
- `config/default-settings.json` — schema v3 defaults (mirrors web client).

## Build (Windows 11 x64, VS2022, CMake 3.26+, Ninja)

```bat
cmake --preset dev
cmake --build --preset dev
ctest --preset dev
```

Qt6 (Widgets) is optional: if not found, `scarlettbridge-ui` is skipped and
everything else still builds. `ci` preset enables warnings-as-errors and
disables Qt. See `../docs/BUILDING.md` for the full toolchain story.

## Boundaries (same as the web prototype)

Bring-your-own-readable-files only. No download/decrypt/unpack, no DRM
workarounds, no SDK material, no keys, no game content, no online-service
interference. Folder validation is read-only and capped, and never writes
into the user's directory.
