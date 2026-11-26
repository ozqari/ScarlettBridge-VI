# LICENSE_MATRIX.md — confirmed via live API + README fetch, 2026-11-26

API license field cross-checked against each README's license section. Full license texts not yet pulled; that happens per-file before any source reuse.

| Code | Confirmed license | Decision |
|---|---|---|
| VIctory, victory-fork, AnyXboxX, Flamingo, VItoriaBr, five-star-recomp, ProjectReCity | GPL-3.0-or-later (API SPDX + README statement agree in all 7) | Derivative runtime reusing any of this code must comply with GPL-3.0-or-later. Preserve notices + license texts. Pinned SHAs in REPO_AUDIT.md. |
| Xbox2Windows | MIT (API + "Original project code and documentation use the MIT License" agree) | May be incorporated into a GPL-3 project with MIT notice retained. Third-party games/assets/SDKs/trademarks excluded (owners retain rights). |
| XCL | GPL-2.0-only (API + README badge + LICENSE agree; Xbox2Windows' own `docs/xcl-reference.md` treats it as a boundary) | INCOMPATIBLE with GPL-3 in one derivative. Reference-only. Options: independent implementation, explicit dual-license permission, or written license review. A process/DLL boundary is NOT an automatic cure. Precedent that permission matters: XCL ships a VIctory port under GPL-2.0-only with the author's permission (2026-11-24, per XCL THIRD_PARTY.md). |
| ScarlettPort | NO single root license (API: "Other"). Fetched `LICENSE.md`: repo combines two GPL runtimes, each part keeps its own license — `bruntime/xcl/` is GPL-2.0-only, second tree is GPL-3-based (fxceless fork). | Per-part compliance required. Never merge the trees. Treat like XCL + GPL-3 side by side. Full LICENSE.md blob saved in `scratch/audit/scarlettport-license.json`. |
| Qt 6, DXC, SDL3/GameInput/XInput, spdlog+fmt, nlohmann/json, Catch2/gtest, ImGui | Per brief §7.2, still to pin at C++ implementation time | Pin versions; verify module license vs static/dynamic distribution before shipping. Smallest set that works. |

Nested / third-party findings (root listings + README hits, live):
- AnyXboxX: `THIRD_PARTY.md` + `.gitmodules` + `third_party/victory` + MIT `libHttpClient` Task module → inspect all three before reuse.
- five-star-recomp: `THIRD_PARTY_NOTICES.md` + `vendor/` per-folder provenance → the model to copy for our own provenance practice.
- XCL: root `THIRD_PARTY.md`.
- ProjectReCity: `.gitmodules` → verify each submodule's license separately; root GPL does not cover submodules by assumption.

Release hygiene (every release): root license matches included source; third-party notices complete; submodules pinned + licensed; no game files/keys/assets/saves/dumps; no secrets or private paths; reproducible build from documented deps.
