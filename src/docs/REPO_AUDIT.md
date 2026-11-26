# REPO_AUDIT.md — live audit, 2026-11-26 (~12:30 UTC)

Method: GitHub REST API per repo (metadata, latest commit, root file listing, README). `raw.githubusercontent.com` is unreachable from this workspace, so READMEs were pulled through `GET /repos/{owner}/{repo}/readme`. Trackers fetched as pages. Reddit `.json` endpoint returned a login-wall page — not retrievable; its comments are not authoritative evidence anyway. Nothing was cloned, built, or executed.

## Result summary

10 of 11 candidate repos exist and were pushed within ~24h of this audit. `JordanFromIT/CGBJ-VIctory` returns API `Not Found` for both repo and commits — confirms the brief's 404 note. DO NOT DEPEND on it.

| Repo | Latest SHA (UTC) | Top lang / root notes | License (API + README agree) | README status claim (self-reported, NOT verified here) |
|---|---|---|---|---|
| Marvisak/VIctory — 12,938★ | `63bcca43c6a0` 2026-11-25T23:27 "update readme" | Rust. Root: `LICENSE`, `README.md`, `Cargo.*`, `rust-toolchain.toml`, `victory.example.toml`, `.victory-root`. No root THIRD_PARTY file. | GPL-3.0. README: "License GPL-3.0-or-later." | Status 2026-11-25: **not playable (~1 world fps)**; boots, menus at title's 30 fps, world loads, intro cutscene plays, reaches prologue. Opus results "unverified against the title". |
| fxceless/victory-fork — 3,194★ | `ceb9e1f7a018` 2026-11-26T11:18 (texture checks, DirectStorage timings) | Rust. Same root layout as upstream. README is ~59 KB. | GPL-3.0. README: "Licensed GPL-3.0-or-later; so is this fork." | Status 2026-11-26 11:50 UTC: **"Playable, slowly": 8–9.5 world fps** at first boat mission (console 30), 10–14 elsewhere (truncated); boot/menus 30 fps; crash tracing + DRED notes. |
| LooperSalty/AnyXboxX — 391★ | `8ed1109fa9b3` 2026-11-26T11:46 (merge rust-full-into-main) | Python (top). Root: `THIRD_PARTY.md`, `.gitmodules`, `Cargo.*`, `pytest.ini`, `requirements*.txt`. Ships `third_party/victory` (VIctory fork) + MIT `libHttpClient` Task module. | GPL-3.0 + nested third-party terms. | **~14 world fps** (Series X target, RTX 4070 SUPER, measured 2026-11-25); 1.0.2.1 build launches, menus 30 fps. "No game files in this repository." |
| JordanFromIT/CGBJ-VIctory | API `Not Found` (repo + commits) | — | Unknown | Unavailable. Excluded. |
| realblastex/flamingo — 53★ | `d50d2545d779` 2026-11-26T10:18 "Handoff docs: status at 9-10 fps…" | Rust. Root adds `FLAMINGO.md`, `flamingo-log.cmd`. Fork of victory-fork. | GPL-3.0. README: "licensed GPL-3.0-or-later, and so is this fork." | Boot/menus 30 fps, cutscenes play, **9–10 fps** handoff note, RTX 4070 + own copy. "No game files, code or assets." Ports ScarlettPort (GPL-3.0) passes. |
| uiliannn/VItoriaBr — 214★ | `1361e190b7f4` 2026-11-26T04:09 | Rust. PT-BR/EN README (~30 KB), `docs/asset-policy.md`. | GPL-3.0. README: GPL-3.0-or-later, no game files shipped. | **3D open world rendering live, up to ~6 fps** (RTX 5060 Ti), menus 30 fps, PortWatch evidence fields; occasional world-transition crashes ("not stable"). |
| ThatRecompDev/five-star-recomp — 220★ | `cfeea3423044` 2026-11-26T12:18 | C#. Root: `THIRD_PARTY_NOTICES.md`, `FiveStar.slnx`, `vendor/` with per-folder provenance. | GPL-3.0-or-later. | Exe + support libs load, imports resolved, boots startup, scripted-gamepad first-run; archive/texture/metadata readers. "No game files, keys or extracted data." |
| R0Y00/Xbox2Windows — 1,962★ | `8c9f168f4746` 2026-11-26T12:12 | Python-top BUT ships `CMakeLists.txt` + `CMakePresets.json` + `AGENTS.md`/`SECURITY.md`. README ~80 KB. | MIT (API + README agree). Third-party games/assets/SDKs belong to owners. | Self-statement: **original-EXE startup, menu nav, gameplay remain unverified**. `docs/xcl-reference.md` records the GPL-2.0-only boundary + pinned revision. HW: RTX 2060 BVH 408-pass, 4070 Ti SUPER report. |
| xcl-dev/xcl — 8,438★ | `b49be1836844` pushed 2026-11-26T06:10 (offline-compute-pipelines merge) | Rust. Root: `THIRD_PARTY.md`, `HANDOFF.md`, `SECURITY.md`. | GPL-2.0-only (API + README badge + LICENSE agree). | Newer retail title **~60% to playable (estimate)**: ~4 s first frame, 30 fps menus, ~5 fps 3D world, 100% shaders translate. Explicit policy: no download/decrypt/unpack (XVC/MSIXVC), no DRM circumvention, no SDK code, no keys, no game data. |
| ZigZagBoi1/ScarlettPort — 383★ | `f4fe7c9b38ce` 2026-11-26T11:51 | Rust. Root license file is `LICENSE.md` (API reports license "Other"). | Mixed per-part: fetched `LICENSE.md` says the repo combines two GPL runtimes, each keeping its own license — `bruntime/xcl/` is **GPL-2.0-only**, second tree GPL-3-based (built on fxceless's GPL-3 fork). No single root license. | Boots → world → gameplay **~5.4 fps** walking/turning (RTX 3060, Series S profile), "not yet verified playable". Bring-your-own-copy; no game files. |
| pixl27/ProjectReCity — 66★ | `b18d4739a0eb` 2026-11-26T09:53 | Python-top. Root: `.gitmodules`, `ROADMAP.md`, `Build.bat`, `make_game.py`. | GPL-3.0-or-later. | Badge: **pre-boot**. No pipelines/draws/present yet. Reads only already-plain packages; no decrypt; no game files/keys/exe. |

## Trackers (fetched live)

- `gtapcport.org` — live. Stages: boots / renders world / player control, with fps + hardware + commits. States **none meets its Playable stage**.
- `tech.ufdmedia.com/trackers/gta-vi-pc-port` — live (Next.js). Stages: Launches / In-world / Parts playable / Groundwork. Notes one contributor report (2 missions in ~45 min at ~2 fps) with **no logged measurement supplied**.
- `vuyp.github.io/gta6-pc-race` — live. 15-minute checks, playable ETA estimate, fps bands (incl. under-2-fps), per-project README-boot claims.
- Reddit thread — JSON endpoint returned a login-wall page. Treat as unretrievable; comments are not evidence either way.

## Disposition (unchanged in direction, now evidence-backed)

- C++ foundation: Xbox2Windows (MIT confirmed, self-declared unverified startup). Preserve MIT notices.
- Runtime behavior: VIctory + victory-fork (both GPL-3.0-or-later confirmed) at the pinned SHAs above — select known-good commit after clean build+test, not newest.
- Instrumentation models: victory-fork (measured ms in commit subjects), flamingo-log.cmd (user log bundles), Xbox2Windows `docs/xcl-reference.md` (GPL-2 boundary pattern).
- Reference only: XCL (GPL-2.0-only triple-confirmed). ScarlettPort: per-part licenses, never merge trees; LICENSE.md fetched and on file in `scratch/audit/scarlettport-license.json`.
- Rejected: CGBJ-VIctory (404 confirmed). No decrypt/unpack/key material anywhere in the audited READMEs — all ten live projects state bring-your-own-copy.

Raw audit payloads: `scratch/audit/` (repo JSON, commit JSON, root listings, README text).
