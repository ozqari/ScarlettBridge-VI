# AGENTS.md — ScarlettBridge agent rules

Rules adapted from brief section 15 for this web workspace.

1. At session start, read AGENTS.md, docs/STATUS_EVIDENCE.md, docs/FEATURE_SELECTION.md, docs/LICENSE_MATRIX.md, and the relevant subsystem design.
2. Check workspace state. Never overwrite unrelated user changes.
3. Work on one milestone and one focused subsystem at a time.
4. Before editing a subsystem, inspect its current code and tests.
5. For every bug, first add a failing synthetic case or reproducible browser test.
6. Keep changes small and conventional (feat/fix/test/docs/build/refactor).
7. Do not make large mechanical refactors while implementing a new feature.
8. Do not add placeholder implementations that return success for unsupported operations. Disabled settings stay disabled with a reason.
9. Do not fake progress, FPS, compatibility, or rendering in production mode. Demo data must be labelled DEMO.
10. Every new runtime behavior needs a test plus a STATUS_EVIDENCE update.
11. Every source-derived change needs a SOURCE_PROVENANCE entry and license check.
12. Run page_refresh, unit checks and relevant integration checks before finishing.
13. Keep the UI responsive; move blocking work to workers or async. Never block the UI thread.
14. Prefer explicit error and result objects and structured logs over silent catch-all handlers.
15. Add bounds checks to every parser and binary-data path.
16. Never run unknown external binaries or repo scripts without inspecting them first.
17. Do not retrieve, decrypt, unpack, distribute or commit game data or keys.
18. Do not patch target executables, disable watchdogs, defeat access controls, or interfere with online protections.
19. At end of each task report: changed files, behavior added, tests run, tests NOT run, remaining risks, next smallest task.
20. Keep public wording title-neutral: describe the platform, component, or fixture tested. Never claim that a game runs.
21. Report capability skips (e.g. exit 77: no D3D12 on this machine) separately from passes and failures. A passing fixture supports the behavior under test; it does not establish full-game startup.
22. Evidence for a hardware-dependent result: commit/build, exact command, observed vs expected, environment (adapter, driver, Windows build, debug-layer), first mismatch or diagnostic, skips, and a negative control for new fixtures.
