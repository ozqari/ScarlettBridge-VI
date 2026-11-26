# TEST_PLAN.md — prototype self-tests + C++ gates

Unit (Benchmark page, Run all checks): settings schema parse, migrate, invalid; hashing (SHA-256 known vector); IPC round-trip + version-mismatch reject + oversize reject; state-machine legal and illegal transitions; bounded-reader overflow, short-input, malformed-offset rejects.

Integration: UI boots with no runtime (Not configured); Start synthetic runtime shows hardware + capability events then logs and progress stream; Stop cleans timers; crash shows UI alive + crash card; reload persists settings; cache clear affects only selected cache key.

GPU (synthetic, labelled per-browser): WebGL2 creation; offscreen-triangle readback hash vs reference; shader compile and link timing; buffer-copy throughput; timestamp query where EXT_disjoint_timer_query_webgl2 exists. WARP, DXC, DRED are not runnable here — DISABLED with reason.

Hardware matrix (C++ gate, not here): RTX 2060-class 8GB, newer NVIDIA, AMD RDNA2+, Intel Arc if feature set permits, WARP clean-install. Record CPU, RAM, GPU, VRAM, driver, Windows, backend, compiler, settings, commit, result — GPU reports additionally identify adapter, driver, Windows build, commit, and debug-layer status. No cross-scene FPS comparisons.

Tool exit codes: 0 pass, 77 capability skip (no D3D12/validation on this machine — reported separately, never a pass), other non-zero = fail with the first mismatch or diagnostic. `run-fixture --census` classifies imports without running anything (negative controls: `unresolved.sbm1` must fail load; corrupt cache entry must be discarded).

Evidence levels per STATUS_EVIDENCE.md. Never label planned work green.
