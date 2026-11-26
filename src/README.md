# ScarlettBridge (web prototype + C++ plan)

**Working name:** ScarlettBridge — rename only after trademark + repo-availability check.
**This workspace:** a perchance.org web prototype of the desktop control center, built directly from `ScarlettBridge_CPP_Runtime_and_GUI_Agent_Brief.md` (research snapshot 26 Nov 2026).

## What this is

- A **mock/runtime-disconnected GUI shell** implementing brief §8 (all 9 pages) with deterministic demo data.
- A **synthetic test foundation** that runs in the browser: WebGL2 device creation, offscreen-triangle pixel readback, shader-compile timing, buffer-copy timing, gamepad polling, settings-schema tests, IPC message round-trip tests.
- The **Phase 0 audit docs** live under `src/docs/` but are snapshot-based and marked unverified until someone re-checks the live repos.

## What this is NOT

- Not a Windows executable, not Qt, not D3D12. The full C++/Qt/D3D12 build (brief §6–7, §13) must happen on a Windows 11 x64 machine with VS2022, CMake, Ninja, vcpkg, Qt 6, Windows SDK + DXC.
- Not able to run commercial console titles. No game files, keys, decryptors, unpackers, or SDK material are present or wanted here.
- Not a playability claim. Every status line uses the brief §12.5 evidence levels; anything not browser-verified is labelled as such in `src/docs/STATUS_EVIDENCE.md`.

## Quick start (web prototype)

1. Open the generator preview. The Dashboard loads in `Not configured` with mock hardware.
2. Use the left rail to visit all 9 pages. Everything works with no folder and no runtime.
3. Dashboard → **Run GPU Test** executes the real WebGL2 synthetic suite and shows pixel-readback results.
4. Dashboard → **Start** launches the simulated runtime host (versioned JSON message stream over an in-page IPC bus); **Stop** shuts it down cleanly; **Simulate crash** proves the UI survives.
5. Local Content Setup → **Validate folder** scans a user-picked local folder read-only (names/sizes/hashes only, capped) and never writes into it.

## Repo map (this workspace)

- `index.html` — entire prototype app (HTML + CSS + JS inline so unsaved preview works).
- `main.pjs` — `$meta` + minimal perchance config only.
- `src/cpp/` — independent C++20 implementation (CMake presets, zero-dep core,
  IPC v1, lifecycle, SBM1 loader, D3D12 smoke path, SBVS translator, Qt shell,
  tools, CPU-only test suite). See `src/cpp/README.md`. Not compiled here —
  builds on Windows 11 x64 with VS2022 (see `src/docs/BUILDING.md`).
- `src/ScarlettBridge_CPP_Runtime_and_GUI_Agent_Brief.md` — authoritative C++ brief (do not edit).
- `src/docs/` — Phase 0 audit + architecture/test/IPC docs for the web prototype and the future C++ tree.
- `src/AGENTS.md` — coding-agent operating rules (from brief §15, adapted for this workspace).

## C++ project (on Windows, not here)

Follow `src/docs/BUILDING.md` (adapted from brief §7/§13) on a real machine: CMake presets `dev`/`release`/`ci`, vcpkg manifest with pinned baselines, x64 only, C++20, `/W4`, Qt 6 Widgets shell + `scarlettbridge-runtime.exe` + optional CLI, named-pipe IPC per `src/docs/IPC_PROTOCOL.md`.

## Boundaries (brief §0 — non-negotiable)

1. No copyrighted game payloads in the repo or releases.
2. Only inspect user-selected, already-readable files. No downloading, decrypting, unpacking, or protection circumvention. No key extraction / firmware exploits / DRM work.
3. No console SDK headers, private binaries, secrets, keys, symbols, or copied proprietary code.
4. No online-service, anti-cheat, auth, or entitlement interference. Offline local testing only.
5. Every source reuse recorded in `src/docs/SOURCE_PROVENANCE.csv`.
6. No fake boot/render/playable claims — label external reports as unverified.
7. No target integrity/watchdog patching.

This file is not legal advice. Stop code reuse and request a license review if anything is unclear.
