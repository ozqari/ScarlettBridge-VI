# Automatic release workflow (`src/tools/github-release/`)

One workflow file, stored here because `src/` forbids dot-directories (so a
literal `.github/` folder cannot live in the workspace).

## Enable (one step, on a machine with git)

```bat
powershell -ExecutionPolicy Bypass -File src/tools/github-release/enable.ps1
git add .github/workflows/release.yml
```

Or by hand: copy `release.yml` in this folder to
`.github/workflows/release.yml` at the repository root (the directory that
contains `src/cpp/CMakeLists.txt`).

## Use

- Cut a release: `git tag v0.2.0 && git push origin v0.2.0` (`v0.2.0-rc1`
  style tags publish as pre-releases automatically).
- Dry run without publishing: Actions → `release` → Run workflow (builds,
  tests, packages and uploads the same zip as a run artifact).

## What the workflow does

`windows-2022` (VS2022 x64) → CMake/Ninja check → Qt 6.8.2 Widgets install
→ vcpkg toolchain check → `cmake --preset ci` (warnings-as-errors, no Qt) →
build → `ctest --preset ci` (CPU-only `sb_tests`; packaging never runs when
tests fail) → `cmake --preset release` (Qt UI when found, skipped gracefully
otherwise) → license/provenance gate (`LICENSE`, `THIRD_PARTY_NOTICES.md`,
`SOURCE_PROVENANCE.csv` must exist) → forbidden-content scan (no game
content, keys or SDK material) → zip + SHA256 + `build-info.txt` → upload
artifacts (30 days) → GitHub Release on tag pushes (zip + checksums +
build info, `certutil` verify instructions in the release body).

Qt is best-effort: when it is unavailable the UI binary is left out and the
release notes say so; core, runtime, tools and tests still ship.

## Status

Written, not yet run — no successful Actions run has been observed. The
first green run (and its log URL) must be recorded in
`src/docs/STATUS_EVIDENCE.md` before any release is treated as verified.
