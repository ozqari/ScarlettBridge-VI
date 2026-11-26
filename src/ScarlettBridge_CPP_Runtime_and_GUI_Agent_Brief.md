# ScarlettBridge — C++ Runtime and Desktop GUI Implementation Brief

**Document type:** Implementation specification for an autonomous coding agent  
**Research snapshot:** 26 November 2026  
**Target platform:** Windows 11 x64 first; Windows 10 compatibility only where the selected APIs and GPU features allow it  
**Primary language:** C++20 (C++23 may be used only after toolchain support is verified)  
**Desktop UI:** Qt 6 Widgets (C++), with an optional Dear ImGui diagnostics overlay  
**Graphics target:** Direct3D 12 first; Vulkan is a later, separately testable backend  
**Working name:** ScarlettBridge (rename only after checking trademarks and repository availability)

---

## 0. Read this before doing anything

You are being asked to build a **new, maintainable C++ Windows compatibility/runtime project with a polished desktop GUI**, informed by the strongest publicly documented ideas from several Xbox-to-Windows projects.

Do not blindly concatenate repositories, mechanically translate every Rust file into C++, or pretend that a full-game PC port already exists. First audit each repository, its current commit, its license, its technical claims, and its actual code. Then design a coherent C++ system and implement it incrementally with tests.

The project must be useful even before it can run a commercial title: it must build cleanly, enumerate hardware, execute synthetic GPU tests, display diagnostics, manage settings, and provide a real GUI in mock/runtime-disconnected mode.

### Non-negotiable boundaries

1. **No copyrighted game payloads in this repository or its releases.** Do not commit game executables, game assets, extracted models, textures, audio, saves, user dumps, or generated files derived from a user's game copy.
2. The runtime may only inspect and use files that the user has independently and lawfully obtained and already made readable. It must not download, decrypt, unpack, or circumvent protections on console packages. Do not implement key extraction, console firmware exploits, security bypasses, DRM circumvention, or instructions for obtaining leaked packages.
3. Do not distribute console SDK headers, private SDK binaries, platform secrets, encryption keys, proprietary symbols, or copied proprietary implementation code. Implement required compatibility behavior independently from public documentation and legally usable material.
4. Do not interfere with online services, anti-cheat, authentication, multiplayer protections, or platform entitlement checks. Scope the first implementation to offline local testing and user-controlled single-player execution.
5. Every source-code reuse decision must be recorded. A public GitHub repository is not automatically permission to copy or translate its code.
6. Do not claim that a test is a game boot, a rendered world, a controllable session, or a playable build unless the test actually demonstrates that exact stage. Label self-reported external results as unverified.
7. Do not disable, patch, or bypass a target application's integrity/watchdog behavior. Fix runtime scheduling and responsiveness in our own implementation; do not manipulate the target to conceal hangs.

This is an engineering brief, not legal advice. If the intended distribution model or any license combination is unclear, stop code reuse and request a license review before proceeding.

---

## 1. Product objective

Create a new C++ application consisting of:

- **ScarlettBridge UI** — a polished desktop control center for setup, hardware inspection, runtime configuration, shader-cache management, diagnostics, logs, and safe launch/stop operations.
- **ScarlettBridge Runtime Host** — a separate C++ process that owns the compatibility runtime and graphics device. It must not require the UI to remain responsive while the runtime is busy or crashes.
- **ScarlettBridge Core** — reusable C++ libraries for runtime services, module loading, graphics translation, shader management, input/audio adapters, and diagnostics.
- **ScarlettBridge CLI** — an optional headless command-line interface for automated tests and reproducible diagnostics.
- **Synthetic Test Suite** — test executables, fake modules, fake shader fixtures, and hardware checks that contain no commercial game content.

The product is a compatibility and research runtime. Do not market it as an official PC port or imply publisher endorsement.

### 1.1 Main success criteria

The first release is successful when all of the following are true:

- A clean clone builds from documented instructions on Windows x64.
- The Qt application starts and works without a game directory.
- Hardware enumeration, D3D12 device creation, WARP smoke tests, logging, settings persistence, and synthetic shader compilation work.
- The UI can launch and monitor the runtime host through a versioned local IPC protocol.
- The runtime can execute synthetic test modules and test platform-service calls without needing commercial game files.
- Every implemented subsystem has unit tests and documented limits.
- Every third-party source and binary dependency has a license/provenance entry.
- A user can select a folder containing already-readable, lawfully obtained files; the app validates basic structure without attempting to obtain, decrypt, or unpack protected packages.
- The app reports exactly what has and has not been tested. It does not invent compatibility or FPS numbers.

### 1.2 Explicit non-goals for the first milestone

- A guaranteed playable GTA VI PC version.
- A universal Xbox emulator.
- A full x86-64-to-C++ decompiler for arbitrary executables.
- A console package decryptor or unpacker.
- A complete replacement for the Xbox GDK or private console SDK.
- Multiplayer, online account, anti-cheat, or platform-service compatibility.
- A Vulkan renderer before the D3D12 path is reliable.
- A promise of 30/60 FPS on any GPU.

---

## 2. Technical model: what should actually be built

### 2.1 Do not confuse native CPU execution with recompilation

Xbox Series X|S software uses an x86-64 CPU architecture. Where the target's executable code and ABI are compatible with the host, CPU instructions can execute natively. The difficult work is the surrounding platform environment: console-specific imports and services, memory behavior, task/thread semantics, graphics API differences, GPU shader translation, resource layouts, audio, input, and content formats.

Therefore, the default architecture should be a **native x64 execution host plus a compatibility layer**, not a plan to decompile the entire game executable into C++ source.

A separate static-recompilation research track may investigate translating small, self-contained x64 functions into C++ or object code. Treat it as an experimental toolchain with its own test corpus. Do not make it a prerequisite for the runtime or imply that converting native x64 instructions into C++ automatically improves performance.

### 2.2 The likely performance-critical areas

Measure before optimizing. The public projects report bottlenecks around:

- Shader translation coverage and correctness.
- Graphics pipeline-state-object (PSO) creation and cache misses.
- RDNA2 wave64 behavior on host GPUs with different native wave widths.
- Heavy mesh/indirect dispatch paths.
- GPU queue synchronization and CPU submission stalls.
- Repeated title-memory-to-GPU copies and descriptor/resource updates.
- Texture streaming and memory residency.
- GPU watchdog/device-removal events caused by long-running or invalid work.

Instrument these separately. Never reduce the diagnosis to a single FPS number.

### 2.3 Runtime execution flow

The intended high-level flow is:

1. The UI validates a selected local folder and displays what it found.
2. The UI launches `scarlettbridge-runtime.exe` with a versioned configuration file and a local named-pipe endpoint.
3. The runtime verifies its own configuration, logs host hardware and graphics capabilities, and initializes the selected backend.
4. The runtime maps supported executable modules and resolves approved compatibility imports.
5. Runtime services provide the supported host-side behavior required by test modules.
6. The graphics adapter translates supported source shader programs to the selected desktop representation and creates native pipelines.
7. The runtime loads user-provided readable data only through the approved local provider interface.
8. The UI receives structured events and displays state, compile progress, timing, resource use, warnings, and crash reports.
9. The runtime exits cleanly or reports a device/runtime failure without taking down the UI.

---

## 3. Research baseline: repositories to examine

**Important:** the status below is a snapshot of public documentation and tracker reports around 26 November 2026. Repository heads can change at any time. Before using anything, fetch the current commit SHA, read its actual `LICENSE`, `README`, contribution guide, third-party notices, and recent history. Treat project performance claims as maintainer-reported unless independently reproduced.

### 3.1 Primary technical references

| Repository | Main value | Reported status in this research snapshot | Reuse / license handling |
|---|---|---|---|
| [Marvisak/VIctory](https://github.com/Marvisak/VIctory) | Base runtime architecture, loader, host services, D3D12 path and shader recompiler | Reaches the game world in its own report; rendering remains incomplete/slow | GPL-3.0-or-later. Reuse only with full GPL compliance and provenance. |
| [fxceless/victory-fork](https://github.com/fxceless/victory-fork) | Rendering and performance fixes on top of VIctory | Reports controllable early scenes and world rendering, with device-loss/stability problems | GPL-3.0-or-later. Strongest candidate for a GPL-compatible runtime baseline if source reuse is chosen. |
| [LooperSalty/AnyXboxX](https://github.com/LooperSalty/AnyXboxX) | Integration work, diagnostics, test organization, shader/texture helpers and measured runs | Its README reports higher performance in selected test scenes on an RTX 4070 SUPER; full playability is not established | GPL-3.0-or-later, with separately licensed third-party components. Inspect `THIRD_PARTY.md` and submodules before reuse. |
| [JordanFromIT/CGBJ-VIctory](https://github.com/JordanFromIT/CGBJ-VIctory) | Linux/Proton experiments and performance branches | A search-indexed README reports world rendering and player control on an AMD GPU under Proton; direct repository retrieval returned 404 during this check, so availability must be re-verified | Reported as GPL-3.0-or-later and a VIctory derivative. Do not depend on it until the live repository, commit and license are confirmed. Port concepts only after verifying their correctness on Windows/D3D12. |
| [realblastex/flamingo](https://github.com/realblastex/flamingo) | Controller integration, OSD/performance display and a more integrated VIctory derivative | Reports controllable early gameplay on one RTX 4070 configuration | GPL-3.0-or-later. Check the exact source revision and third-party dependencies. |
| [uiliannn/VItoriaBr](https://github.com/uiliannn/VItoriaBr) | Rendering, lighting, memory/VRAM and DXIL optimization work | The reported state varies by branch and hardware; validate each improvement independently | GPL-3.0. Derivative source reuse requires GPL compliance. |
| [ThatRecompDev/five-star-recomp](https://github.com/ThatRecompDev/five-star-recomp) | Separate loader/static-recompilation approach, shader/data pipeline and archive/texture readers | Reports loader progress, startup screens and partial world image; not a complete playable runtime | GPL-3.0-or-later. Primarily C#/.NET; treat as architecture and test-reference material unless a deliberate GPL-compatible C++ port is approved. |

### 3.2 C++ and broader compatibility references

| Repository | Main value | Current limitation | Reuse / license handling |
|---|---|---|---|
| [R0Y00/Xbox2Windows](https://github.com/R0Y00/Xbox2Windows) | Native C++20/CMake, D3D12, DXC, WARP/hardware tests, small static CPU recompilation experiments and shader fixture verification | Its own README says full target executable startup and a verified rendered game frame remain unproven | MIT. This is the best directly relevant C++ engineering foundation; preserve MIT notices and audit each dependency. |
| [xcl-dev/xcl](https://github.com/xcl-dev/xcl) | Broad Xbox compatibility-layer architecture, import/service modeling, and behavior from other real retail titles | Mostly Rust; not a direct C++ implementation base | GPL-2.0-only. Use as a behavioral/design reference only unless explicit compatible permission is obtained. Do not combine its code into a GPL-3-derived runtime by assumption. |
| [ZigZagBoi1/ScarlettPort](https://github.com/ZigZagBoi1/ScarlettPort) | Launcher/runtime separation, runtime selection, logs, scripts, task board, and orchestration of two runtime lines | It contains separate XCL and VIctory runtime trees with different licenses; progress is self-reported | Its launcher is Rust/egui. Use the product/architecture ideas. Do not merge its XCL GPL-2.0-only code with GPL-3 code without legal permission. |
| [pixl27/ProjectReCity](https://github.com/pixl27/ProjectReCity) | Static-recompilation toolchain structure, shader/asset tooling, Vulkan research, and detailed test/roadmap organization | Its README says the game does not boot yet | GPL-3.0-or-later and derived from Leonida. Reference its documented work; do not assume every submodule or generated table shares the root license. |

### 3.3 What to combine, and what not to combine

Use the following priority order:

1. **C++ foundation:** Xbox2Windows for its existing C++/CMake/D3D12 test discipline, small CPU translation experiments, shader fixture tests, WARP checks, and hardware-report format. Do not mistake its research state for a working full-game runtime.
2. **Runtime behavior:** VIctory and victory-fork for the leading Windows execution/runtime model. Select the current known-good commit after a clean build and test, rather than just taking the newest commit.
3. **Performance experiments:** AnyXboxX and CGBJ-VIctory for instrumentation and optimization hypotheses. Port each optimization separately and require before/after measurements.
4. **NVIDIA and controller polish:** Flamingo and VItoriaBr for hardware-specific findings, gamepad behavior, overlay concepts, and rendering fixes. Avoid adopting any single-GPU workaround as a universal implementation.
5. **Alternative loader/data pipeline:** Five Star Recomp and Project ReCity as separate architectural references. Do not collapse a different static-recompilation toolchain into the runtime until its function and license are understood.
6. **Cross-check only:** XCL for documented import behavior and alternate results. It is GPL-2.0-only, so do not copy or mechanically translate its source into a GPL-3 runtime without explicit permission/dual licensing and a license review.
7. **GUI inspiration:** ScarlettPort's launcher layout and Flamingo's overlay are references. Build the actual GUI in C++/Qt rather than adding Rust/egui to the shipped product.

**Do not take the largest repository as the source of truth.** Prefer small, verified changes with a known origin, test case, and measured result.

---

## 4. Mandatory repository audit before implementation

Create a `research/` directory outside the shipping source tree. Clone or inspect the repositories in read-only mode. Do not automatically execute their build scripts, release binaries, setup scripts, or package installers.

For every candidate repository:

1. Record the canonical URL, owner/repository, default branch, commit SHA, tag/release, last commit date, language breakdown, and current README status.
2. Read the root license and any nested license files.
3. Read `THIRD_PARTY.md`, `THIRD_PARTY_NOTICES.md`, submodule manifests, generated-code notices, and bundled asset policies.
4. Identify the exact folders and files relevant to the proposed contribution.
5. Determine whether the reported improvement is present in a public commit, a public branch, an unmerged PR, or merely a Reddit/Discord statement.
6. Build only in an isolated development environment after the scripts and dependencies have been reviewed.
7. Record commands, toolchain versions, hardware, driver, configuration, results, and failures.
8. Keep source repos read-only. Do not modify an upstream clone and then forget which changes were yours.
9. Make a minimal patch against a fixed commit and add a test that proves the intended behavior.
10. Re-run the test on a clean checkout and preserve the exact commit and test log.

### 4.1 Required audit documents

Generate these before copying or porting code:

- `docs/REPO_AUDIT.md` — technical summary, activity, evidence quality, buildability and candidate feature list.
- `docs/LICENSE_MATRIX.md` — root and nested license details, compatibility decision, third-party conditions and attribution obligations.
- `docs/SOURCE_PROVENANCE.csv` — original repository, SHA, path, license, upstream author/commit, destination path, and whether code was copied, ported, independently implemented, or only used as behavioral reference.
- `docs/FEATURE_SELECTION.md` — chosen implementation items, rejected alternatives, reason for selection, test plan and expected measurable benefit.
- `docs/STATUS_EVIDENCE.md` — separate columns for implemented, unit-tested, synthetic-test verified, hardware verified, target-startup verified, and gameplay verified.

No source code reuse before these documents exist and the license decision is recorded.

---

## 5. Copyright, source provenance and licensing plan

### 5.1 Recommended licensing direction

If the project reuses code from VIctory, victory-fork, AnyXboxX, Flamingo, VItoriaBr or Five Star Recomp, the safest initial assumption is that the resulting derivative runtime must comply with **GPL-3.0-or-later**, subject to file-level and third-party exceptions. Preserve original copyright notices and license texts.

Xbox2Windows is marked MIT. Its code can generally be incorporated into a GPL-3.0 project if the MIT license and copyright notice are retained and all third-party requirements are also satisfied.

XCL is marked **GPL-2.0-only**. GPL-2.0-only and GPL-3.0-or-later code cannot simply be combined into one derivative program. ScarlettPort documents keeping its XCL and VIctory runtime trees separately with their own licenses. For this project, either:

- use XCL only as a behavioral/specification reference and independently implement equivalent behavior from public information;
- obtain explicit permission or a compatible dual license from the relevant copyright holders; or
- obtain a written license review for a genuinely separate-component architecture before distributing anything.

A process boundary is not an automatic legal cure. Do not assume that placing components in different DLLs, processes, folders, or repositories makes any license conflict disappear.

### 5.2 Rules for code porting

- Never remove copyright headers, attribution, license files, or required notices.
- Never copy code from a repository with no license or unclear ownership.
- Do not mechanically translate Rust/C# code into C++ to disguise its origin. A translated derivative remains subject to applicable licensing obligations.
- For MIT/Apache/BSD components, preserve the relevant notices and include all required license/NOTICE files.
- Keep a file-level provenance map for every copied or ported implementation.
- If two implementations disagree, build a synthetic test to adjudicate; do not take a majority vote from forks.
- Do not copy game assets, private console SDK material, generated proprietary symbols, or user-specific data.

### 5.3 Release hygiene

Before every public release, verify:

- Root license matches the actual included source.
- `THIRD_PARTY_NOTICES.md` is complete.
- Nested dependencies and submodules are pinned and licensed.
- No game files, keys, assets, extracted data, saves, or user dump paths are included.
- No secrets, credentials, crash dumps containing private memory, or personal paths are included.
- Build artifacts are reproducible from source and publicly documented dependencies.

---

## 6. Proposed architecture

### 6.1 Processes and components

Use two primary processes initially:

**A. `scarlettbridge-ui.exe`**

- Qt 6 Widgets application.
- Owns the window, settings editor, configuration manager, runtime lifecycle controller, and user-facing diagnostics.
- Must remain responsive if the runtime is compiling shaders or crashes.
- Does not own the game/runtime GPU device.

**B. `scarlettbridge-runtime.exe`**

- Native C++20 process.
- Owns the runtime, module loader, platform service adapters, graphics device, shader cache, and all runtime threads.
- Emits structured events to the UI.
- Can run headless from CLI for automation.

Use a **local Windows named pipe** with an explicit protocol version and current-user ACL for UI/runtime communication. Do not expose an unauthenticated TCP control server. Validate every message and cap message sizes.

An optional `scarlettbridge-cli.exe` should use the same core libraries and configuration schema, not a second implementation.

### 6.2 Core library boundaries

Keep modules isolated behind stable C++ interfaces. Avoid circular dependencies.

```text
ScarlettBridge/
├── CMakeLists.txt
├── CMakePresets.json
├── vcpkg.json
├── AGENTS.md
├── README.md
├── LICENSE
├── THIRD_PARTY_NOTICES.md
├── docs/
│   ├── ARCHITECTURE.md
│   ├── BUILDING.md
│   ├── REPO_AUDIT.md
│   ├── LICENSE_MATRIX.md
│   ├── SOURCE_PROVENANCE.csv
│   ├── FEATURE_SELECTION.md
│   ├── STATUS_EVIDENCE.md
│   ├── IPC_PROTOCOL.md
│   ├── TEST_PLAN.md
│   └── CHANGELOG.md
├── config/
│   └── default-settings.json
├── src/
│   ├── app/
│   │   ├── ui_main.cpp
│   │   ├── runtime_main.cpp
│   │   └── cli_main.cpp
│   ├── ui/
│   │   ├── app_shell/
│   │   ├── dashboard/
│   │   ├── library_setup/
│   │   ├── graphics_settings/
│   │   ├── shader_cache/
│   │   ├── diagnostics/
│   │   ├── logs/
│   │   ├── controls/
│   │   └── widgets/
│   ├── core/
│   │   ├── config/
│   │   ├── result/
│   │   ├── logging/
│   │   ├── events/
│   │   ├── tasking/
│   │   └── versioning/
│   ├── ipc/
│   │   ├── protocol/
│   │   ├── named_pipe/
│   │   └── messages/
│   ├── runtime/
│   │   ├── lifecycle/
│   │   ├── title_profile/
│   │   ├── memory/
│   │   ├── threads/
│   │   ├── file_access/
│   │   ├── module_loader/
│   │   ├── imports/
│   │   └── diagnostics/
│   ├── platform/
│   │   └── windows/
│   │       ├── memory/
│   │       ├── synchronization/
│   │       ├── filesystem/
│   │       ├── audio/
│   │       ├── input/
│   │       └── system_services/
│   ├── graphics/
│   │   ├── abstraction/
│   │   ├── d3d12/
│   │   ├── shader_ir/
│   │   ├── shader_translation/
│   │   ├── pipeline_cache/
│   │   ├── resource_tracking/
│   │   └── presentation/
│   ├── media/
│   │   ├── audio/
│   │   └── input/
│   ├── formats/
│   │   ├── textures/
│   │   ├── metadata/
│   │   └── archives_readonly/
│   └── telemetry/
│       ├── metrics/
│       ├── trace/
│       └── crash_reports/
├── tools/
│   ├── gpu_probe/
│   ├── shader_probe/
│   ├── fixture_builder/
│   └── cache_inspector/
└── tests/
    ├── unit/
    ├── integration/
    ├── gpu/
    ├── fixtures/
    └── synthetic_titles/
```

Adapt the layout only when a real dependency requires it. Keep all application/runtime code under `src/`; keep docs and build metadata in the root/docs as shown.

### 6.3 Core interfaces

Create interfaces before concrete implementations:

- `IRuntimeHost` — initialize, start, request stop, shutdown, current state.
- `IPlatformServices` — host implementations for explicitly supported platform calls.
- `IModuleLoader` — validates and maps approved test modules; records every unresolved import.
- `IGraphicsBackend` — adapter selection, device creation, resource allocation, queues, barriers, pipeline creation, presentation, timing and device-loss reporting.
- `IShaderTranslator` — validates input, translates to an intermediate form, compiles, validates output, emits a structured report.
- `IAssetProvider` — reads from a user-selected local folder only; never downloads content.
- `IAudioBackend` — initializes audio and provides supported voice operations.
- `IInputBackend` — keyboard, mouse and gamepad events.
- `IDiagnosticsSink` — structured counters, logs, traces, crash/device-loss events.

Each interface must return explicit error/result types. Do not use exceptions as normal control flow across module boundaries.

---

## 7. C++ toolchain and dependency choices

### 7.1 Build system

- C++20 as the minimum language standard.
- Visual Studio 2022 / MSVC x64 toolchain, with clang-cl available for additional analysis if practical.
- CMake presets for `dev`, `release`, and `ci`.
- Ninja generator in documented developer instructions.
- vcpkg manifest mode with exact dependency baselines pinned.
- Build x64 only initially; do not add ARM64 until the design is stable.
- Enable `/W4`, selected `/permissive-`, `/EHsc`, and useful static analysis. Warnings must be treated as errors in CI after baseline cleanup.

### 7.2 Main dependencies

Use the smallest dependency set that satisfies the project:

| Dependency | Purpose | Rule |
|---|---|---|
| Qt 6 Widgets | Main desktop GUI | Pin a stable Qt 6 release; verify license for the chosen modules and static/dynamic distribution model. |
| Direct3D 12 / DXGI | Primary Windows renderer | Use public Windows SDK headers and documented interfaces. |
| DXC | HLSL-to-DXIL compilation/validation | Use a documented public distribution and pin tool version. |
| SDL3 or Windows GameInput/XInput | Controller and input abstraction | Pick one primary path and keep platform adapters isolated. |
| spdlog + fmt | Structured logging | Pin versions and license notices. |
| nlohmann/json | Settings and IPC serialization | Use strict schema/version validation and size limits. |
| Catch2 or GoogleTest | Unit/integration tests | Choose one; avoid duplicate frameworks. |
| Dear ImGui (optional) | Developer-only in-runtime overlay | Keep optional and separate from the main Qt application. |

Do not add a package simply because an upstream project happens to use it. Audit dependency licenses and security advisories. Use system Windows components where appropriate rather than redistributing them.

### 7.3 Renderer strategy

- **Phase 1:** D3D12 on Windows. This matches the most advanced Windows-oriented runtime experiments and provides access to DRED/device-removal diagnostics.
- **Phase 2:** Add a Vulkan backend only if it has a clear requirement and can pass the same backend conformance tests.
- Keep all backend-independent resource and shader interfaces outside `graphics/d3d12/`.
- Do not write the entire runtime directly against Qt or a GUI framework.

---

## 8. Desktop GUI specification

The GUI is a first-class deliverable, not a placeholder window. Build it early using mock data so visual and interaction design can be completed independently of the runtime.

### 8.1 Visual direction

- Modern, dark, technical desktop application.
- Charcoal background, restrained blue/cyan accent, readable contrast, clear status colors.
- Left navigation rail with icons and text.
- Compact top bar containing runtime state, selected GPU, and app version.
- Main content area with responsive cards and tables.
- No unnecessary giant hero graphics or fake FPS counters.
- Consistent spacing, typography, icon sizes, disabled-state behavior, tooltips and keyboard navigation.
- Must remain usable at 1280×720 and scale cleanly to 1920×1080 and 4K.
- Store UI preferences separately from runtime configuration.

### 8.2 Pages and exact functionality

#### Page 1 — Dashboard

Display:

- Runtime state: Not configured / Ready / Initializing / Running / Stopping / Crashed.
- Selected content folder and validation state.
- Selected graphics adapter and driver version.
- CPU model, logical processors and RAM summary.
- GPU VRAM capacity and currently available budget where queryable.
- Last run result and timestamp.
- Last crash/device-loss result.
- Start, Stop, Open Logs, Run GPU Test and Settings actions.

Never show a green check for a feature that has only been planned. Every status needs a source: runtime event, local file validation, synthetic test, or verified target test.

#### Page 2 — Local Content Setup

- Folder picker for a user-selected, already-readable directory.
- Read-only scan of file names, sizes and hashes needed for version/fingerprint reporting.
- Display missing expected files and unsupported folder layouts.
- Show a plain-language notice that the program does not download, decrypt, unpack or bypass protection.
- No browse-to-download buttons, no key fields, no package decryption workflow.
- Provide a “Validate folder” button and detailed validation log.
- Allow the user to remove the stored path without deleting user files.

#### Page 3 — Graphics Settings

- Adapter selection (when multiple adapters exist).
- Backend selector: D3D12 initially; Vulkan disabled until implemented and tested.
- Window mode, output resolution, VSync, frame pacing and optional frame cap.
- Texture/streaming budget controls with safe defaults.
- Shader cache enable/size/path.
- Precompilation toggle for supported, legally accessible shader inputs.
- Optional debug layer and DRED controls.
- Reset-to-default button and per-setting explanations.

Do not expose unsupported toggles. A setting is disabled until its behavior is implemented and tested.

#### Page 4 — Shader Manager

- Number of discovered shader records, translated shaders, failed translations, cache hits/misses and pending jobs.
- Progress bar and estimated remaining work only when a defensible estimate exists.
- Search/filter by shader identifier, stage, status and error category.
- Show input hash, translator version, backend version, output hash and compile duration.
- Clear cache with an explicit confirmation showing the exact cache path and size.
- Export diagnostic reports without exporting game shader binaries or proprietary data.

#### Page 5 — Live Diagnostics

- CPU frame time, GPU frame time, present interval and frame pacing graph.
- GPU queue depth, submission latency, resource-copy bytes, synchronization wait time.
- Memory usage and budget; texture streaming queue size.
- PSO cache hit rate and pipeline creation time.
- Shader compile latency and refusal/error count.
- Device removal reason and DRED summary when available.
- Pause/resume collection, save trace and reset counters.

Distinguish actual FPS from synthetic benchmark throughput. Show a rolling average and percentile distribution instead of only a maximum.

#### Page 6 — Runtime Console and Logs

- Live structured logs with severity, timestamp, subsystem and message.
- Filter by subsystem/severity and search text.
- Copy selected lines; open the log directory.
- Display crash ID, runtime version, GPU driver, settings profile and last events before failure.
- Redact user folder paths in any report intended for sharing; allow user review before export.

#### Page 7 — Controls

- Keyboard/mouse mapping display.
- Gamepad detection and controller test panel.
- Dead-zone and sensitivity settings only when implemented.
- Show the active device and most recent input event.
- Do not install drivers or modify system-wide controller settings.

#### Page 8 — Hardware Benchmark

- Device and feature-level report.
- D3D12 adapter/device creation test.
- WARP synthetic test.
- Synthetic shader compilation test.
- Synthetic resource allocation/copy/barrier test.
- Optional repeatable GPU timing test.
- Export JSON/CSV report with hardware, driver, runtime build and test version.

This page must work without any commercial game files.

#### Page 9 — About / Updates

- App version, runtime version, build commit and license information.
- Links to project documentation and issue tracker.
- Update check for this project's own signed release artifacts only, if an updater is eventually implemented.
- Do not download third-party game builds, game content or arbitrary executables.

### 8.3 GUI behavior requirements

- All long-running work is asynchronous; never block the UI thread.
- Every modal dialog has a clear cancel path unless cancellation would corrupt state.
- Runtime state changes must be event-driven and reflected in the UI.
- Persist only user preferences and local paths; never persist sensitive tokens or private memory dumps.
- Use high-DPI scaling and keyboard accessibility.
- Add a demo mode with deterministic fake runtime events for UI development and screenshots.
- Add UI tests for navigation, settings persistence, runtime disconnected state, runtime crash, and long shader queues.

---

## 9. Runtime and compatibility implementation plan

### 9.1 Runtime lifecycle

Implement a formal state machine:

`Created → Validating → InitializingPlatform → InitializingGraphics → LoadingModules → Ready → Running → Stopping → Stopped`

Any state may transition to `Failed` with a structured reason. A restart must create a fresh runtime instance with a clean device/resource lifetime. Do not leave detached worker threads or GPU objects alive after shutdown.

### 9.2 Module loader and ABI layer

Start with synthetic modules built by the project itself. The loader should:

- Validate architecture and basic executable structure.
- Map permitted test modules with explicit bounds checking.
- Build an import table from the test manifest.
- Resolve only implemented compatibility imports.
- Record unresolved imports with module, symbol, call site and severity.
- Fail clearly when a required import is missing.
- Avoid silently returning success for unknown functions.
- Have deterministic tests for relocation, import resolution, exception propagation and unload.

Do not copy console SDK headers or private imports from leaked SDK binaries. Define only the minimal ABI data structures supported by public documentation and project-owned tests.

### 9.3 Platform services

Build separate, testable adapters for:

- Memory allocation and virtual-memory behavior.
- Thread creation, synchronization primitives and wait semantics.
- Timers and monotonic clocks.
- Filesystem and asynchronous file I/O over the approved local data provider.
- Task queues and async completion callbacks.
- Keyboard, mouse and gamepad input.
- Audio device and voice operations.
- User/session identity only as an offline local stub, with no online authentication impersonation.

For every service, document:

- Supported calls and semantics.
- Unsupported calls.
- Error codes and failure behavior.
- Thread-safety model.
- Tests and known differences from the console environment.

### 9.4 Memory model

- Use checked arithmetic for every allocation and range operation.
- Track allocation ownership and lifetime.
- Avoid raw pointer arithmetic outside a small, reviewed memory layer.
- Track host memory, GPU-local memory and upload/readback memory separately.
- Respect adapter memory budgets; do not assume that all physical VRAM is available.
- Provide configurable low-VRAM behavior and an 8 GB test profile.
- Record large and repeated copies.
- Create a leak test and a stress test for allocation/free churn.

### 9.5 Graphics API translation

Build a compatibility abstraction rather than exposing platform-specific D3D12 objects to every subsystem.

Implement and test in this order:

1. Adapter enumeration and feature query.
2. Device creation and debug-layer setup.
3. Command queues and fences.
4. Resource creation and lifetime tracking.
5. Resource states and barriers.
6. Descriptor heaps and descriptor copying.
7. Upload/readback buffers and staging.
8. Root signatures and pipeline layouts.
9. Graphics and compute PSOs.
10. Draw, dispatch and indirect execution paths.
11. Queries, timestamps and readback.
12. Swapchain and presentation.
13. Device-removal detection and DRED capture.

Every feature needs a synthetic test before it is used by the title runtime.

### 9.6 Shader translation pipeline

Design a pipeline with independently testable stages:

`Source bytes → validation → disassembly/decode → normalized IR → optimization/legalization → target HLSL/DXIL → DXC compile → validation → native PSO`

Requirements:

- Stable shader identifiers and content hashes.
- Versioned IR and cache schema.
- Deterministic translation tests.
- Explicit support matrix for each instruction and shader stage.
- Structured “unsupported instruction” reports.
- Strict validation of bounds, lane widths, descriptor indices and resource bindings.
- Wave64 handling must be a documented, separately tested feature; do not claim full correctness based on one shader.
- Keep the translator independent from the GUI and runtime lifecycle.
- No network service is needed for shader compilation.

Start with synthetic shaders and public instruction documentation. Build a test corpus that validates output pixels on WARP and physical GPUs. Do not copy a protected shader corpus or ship translated game shader files.

### 9.7 Texture and data-format readers

Only implement readers for data already present in an approved, readable user folder. Use explicit bounded parsing:

- Validate magic, header version, lengths, offsets, alignment and checksums where available.
- Reject offsets outside the source file.
- Guard decompression against oversized output and integer overflow.
- Record the format and parser version.
- Use synthetic fixture files generated by the project.
- Keep all output local and never upload or distribute decoded content.

Start with generic texture decoding and public block-compression formats as isolated libraries. Add title-specific metadata support only after an independent legal and technical review.

### 9.8 Audio and controller

- Use a clean audio abstraction with an initial Windows backend.
- Add format decoding only for legally readable, documented inputs.
- Support keyboard and gamepad input through a common event model.
- Add hot-plug support, device change events and controller tests.
- Keep input mapping in JSON and show it in the GUI.

---

## 10. Performance work: port ideas, not unverified patches

The following are optimization hypotheses observed in public project work. Implement them only after a baseline has been captured and a test proves correctness.

### 10.1 Pipeline prewarming and cache

- Record all pipeline descriptions encountered in a run.
- Build a deterministic cache key from shader hashes, root signature/layout, format state, backend and compiler versions.
- Reuse valid cached pipelines across runs.
- Add a prewarm queue for supported, already-readable shader inputs.
- Report cache hits, misses, compilation latency and failures.
- Never substitute an invalid pipeline silently.

### 10.2 Submission overlap

The CGBJ-VIctory public branches describe experiments that prepare upcoming work while previous GPU work is executing. Implement a scheduler with explicit ownership and fences. Prove that command submission order and memory dependencies remain correct. Add a test that compares serial and overlapped output hashes.

### 10.3 Reduce CPU/GPU copy overhead

- Track every copy in bytes and time.
- Detect unchanged pages/buffers where the runtime's own memory model permits it.
- Use ring buffers and batching for upload traffic.
- Avoid copying the same immutable data repeatedly.
- Only implement dirty-page/mirror caching when invalidation is correct and race-tested.
- Add a per-frame copy budget and report spikes.

### 10.4 Indirect argument validation

If a supported translation path clamps or normalizes indirect arguments, do so in a validated pre-pass only when the source semantics require it. Do not skip a GPU operation and leave uninitialized output that a later indirect draw could consume. Add fuzz tests for argument ranges.

### 10.5 Shader work scheduling

- Separate translation from native PSO creation.
- Allow background compilation only when resource lifetimes and thread-safety are defined.
- Prioritize currently required shaders without starving cache maintenance.
- Report compile progress to the GUI.
- Handle the target application's expected service responsiveness without patching the target's watchdog or integrity logic.

### 10.6 Device stability

- Capture DRED/device-removal reason.
- Log the last submitted command list and pipeline identifiers.
- Track long GPU operations and driver resets.
- Add a recovery boundary that tears down and recreates the graphics device only when safe.
- Never report a recovered device as a successful game session if the target state was lost.

### 10.7 Low-VRAM profile

Include a conservative 8 GB VRAM profile because the project must test on mid-range hardware, not only 16–24 GB cards. Provide:

- Explicit streaming budget.
- Configurable shader-cache disk budget.
- Bounded upload/readback pools.
- Back-pressure when memory budgets are reached.
- Clear diagnostic messages when a feature is disabled due to a hardware limit.

Do not promise a specific FPS for an RTX 2060 Super. First deliver a GPU capability report and synthetic performance numbers; actual title performance requires a separate reproducible test.

---

## 11. IPC protocol between UI and runtime

Use a versioned local protocol. Initial messages may be JSON for development; if performance later requires a binary protocol, keep the same message semantics and versioning.

Required message families:

- `Hello` / `HelloAck` — protocol version, runtime build ID, supported features.
- `StartRuntime` / `StartAccepted` — validated config reference and selected profile.
- `StopRuntime` / `StopAccepted` — graceful shutdown request.
- `RuntimeState` — current lifecycle state and reason.
- `HardwareInfo` — CPU, GPU, driver, memory and feature support.
- `LogEvent` — timestamp, severity, subsystem, message and correlation ID.
- `ProgressEvent` — operation, completed count, total count and phase.
- `ShaderEvent` — input hash, stage, status, compile duration and error code.
- `FrameStats` — CPU/GPU/present timings and counters.
- `DeviceLost` — removal reason, DRED availability and crash correlation ID.
- `UserActionResult` — success/failure and safe explanation.

Requirements:

- Protocol version must be checked at handshake.
- Reject unknown required fields and invalid enum values.
- Cap message size and queue length.
- Avoid leaking arbitrary memory contents in diagnostics.
- Use current-user ACLs on the named pipe.
- Never accept commands from remote network clients.
- Add fuzz tests for malformed, truncated and oversized messages.

---

## 12. Testing and verification strategy

### 12.1 Unit tests

At minimum:

- Settings schema parsing, version migration and invalid values.
- Hashing and file fingerprinting.
- PE/module header validation using project-owned fixtures.
- Import-table resolution against synthetic functions.
- Memory range, alignment and overflow checks.
- Synchronization and task queue behavior.
- Texture decoder bounds and malformed-input rejection.
- Shader IR verification and deterministic output.
- Pipeline-cache key stability and corruption handling.
- IPC parser/serializer round trips.
- UI model state transitions.

### 12.2 Integration tests

- UI starts with no runtime and shows “Not configured”.
- UI launches a synthetic runtime host.
- Runtime reports hardware and D3D12 capability information.
- UI receives logs and progress events.
- Stop request shuts down all runtime threads cleanly.
- Runtime crash is displayed without crashing the UI.
- Reopen UI and settings persist.
- Cache deletion only affects the selected cache directory.

### 12.3 GPU tests

- Adapter enumeration.
- D3D12 device creation.
- WARP root-signature, resource allocation, resource copy, barrier and offscreen render tests.
- DXC shader compilation and validation.
- Shader output pixel comparison.
- GPU timestamp and pipeline-cache test.
- Device-removal diagnostic test where safely reproducible.

Keep hardware-dependent tests clearly marked. WARP success is not proof that a real GPU behaves identically.

### 12.4 Hardware matrix

Minimum target test categories:

- NVIDIA Turing / RTX 20 series, including an RTX 2060-class 8 GB card.
- NVIDIA Ampere and newer.
- AMD RDNA2 and newer.
- Intel Arc where the selected D3D12 feature set permits.
- WARP software adapter for deterministic smoke tests.

For every report, include CPU, RAM, GPU, VRAM, driver, Windows build, backend, compiler version, settings, test commit, and result. Never compare FPS without matching scene/workload and configuration.

### 12.5 Evidence levels

Every feature/status line in README and GUI must use one of these levels:

1. **Implemented** — code exists.
2. **Unit-tested** — unit tests pass.
3. **Synthetic verified** — project-owned integration fixture passes.
4. **Hardware verified** — verified on named hardware/backend.
5. **Target startup verified** — a supported user-provided target reaches the exact documented startup milestone.
6. **World rendering verified** — a real 3D frame is produced and evidence retained locally.
7. **Controllable session verified** — input reaches the target and the player can control it.
8. **Sustained session verified** — stability duration and repeatability are documented.
9. **Playable** — the defined playable test suite passes; do not use this label for a short cutscene or one scene.

The public trackers report different stages and often rely on project self-reporting. Our README must not repeat these claims as independently verified facts.

---

## 13. CI, release engineering and documentation

### 13.1 Continuous integration

Create Windows CI that:

- Configures CMake from a clean checkout.
- Restores pinned dependencies.
- Builds Debug and Release.
- Runs all CPU-only unit tests.
- Runs synthetic GPU tests on WARP where available.
- Packages the UI and runtime separately.
- Performs license/provenance checks.
- Runs formatting and static analysis.
- Uploads logs for failed CI jobs, but never game data or proprietary files.

Hardware-specific GPU tests can run in a separate opt-in/self-hosted workflow. Never claim that a cloud CI runner verified a discrete GPU feature unless the runner actually has that hardware.

### 13.2 Documentation files

Maintain:

- `README.md` — what the project is, what works, build instructions and clear limitations.
- `AGENTS.md` — rules for coding agents, build/test commands, file boundaries and safety limits.
- `docs/ARCHITECTURE.md` — component boundaries and runtime data flow.
- `docs/BUILDING.md` — exact compiler/SDK/dependency setup.
- `docs/IPC_PROTOCOL.md` — all messages and versioning rules.
- `docs/TEST_PLAN.md` — tests, hardware matrix and evidence levels.
- `docs/REPO_AUDIT.md` — repository research.
- `docs/LICENSE_MATRIX.md` — licensing analysis.
- `docs/SOURCE_PROVENANCE.csv` — file-level attribution map.
- `docs/FEATURE_SELECTION.md` — features chosen from candidate projects.
- `docs/STATUS_EVIDENCE.md` — current actual status.
- `docs/CHANGELOG.md` — every meaningful change.
- `docs/TROUBLESHOOTING.md` — common build/runtime/device errors.

Keep status documentation current after each meaningful milestone. Do not let marketing claims outpace test evidence.

---

## 14. Phased implementation roadmap and task breakdown

Do not attempt every subsystem in one giant change. Use small, buildable milestones. After each milestone, run the full relevant tests, update the changelog and status evidence, and commit the work.

### Phase 0 — Research and license gate

Tasks:

1. Create the empty repository and `research/` workspace.
2. Inspect the listed candidate repositories at fixed commit SHAs.
3. Record language, architecture, current status, licenses and third-party dependencies.
4. Identify which source can be reused, which may only be referenced, and which must be excluded.
5. Generate `REPO_AUDIT.md`, `LICENSE_MATRIX.md`, `SOURCE_PROVENANCE.csv`, and `FEATURE_SELECTION.md`.
6. Select the initial source-reuse policy and root license.
7. Stop and request legal review if a proposed source mix is incompatible or unclear.

**Exit gate:** no unreviewed code copied into `src/`; all candidates have a license/provenance disposition.

### Phase 1 — C++ project skeleton

Tasks:

1. Create root CMake project and presets.
2. Add vcpkg manifest and pin dependency versions.
3. Add `core`, `ipc`, `runtime`, `graphics`, `ui`, `tools` and `tests` targets.
4. Build a minimal runtime executable that prints version/build ID and exits.
5. Build a Qt GUI executable that opens a dark main window.
6. Add structured logging and JSON settings persistence.
7. Add formatting/static-analysis configuration.
8. Add CI that builds and runs trivial tests.

**Exit gate:** clean-clone build passes in CI; both executables open/run.

### Phase 2 — GUI shell in mock mode

Tasks:

1. Implement navigation sidebar and top status bar.
2. Implement Dashboard with deterministic fake data.
3. Implement Local Content Setup with folder picker and read-only validator stub.
4. Implement Graphics Settings and persistence.
5. Implement Shader Manager with synthetic job data.
6. Implement Diagnostics graph widgets with fake metrics.
7. Implement Logs view with severity filters/search.
8. Implement About page and license display.
9. Add high-DPI and 1280×720 layout tests.
10. Add UI demo mode controlled by command-line flag.

**Exit gate:** all pages work with no runtime, no game folder and no external assets.

### Phase 3 — Hardware probe and D3D12 smoke tests

Tasks:

1. Enumerate DXGI adapters.
2. Query driver version, feature levels, shader model and relevant capabilities.
3. Report adapter memory budgets.
4. Create a D3D12 device with debug options.
5. Add WARP device selection.
6. Test queues, fences, resource allocation, copies and barriers.
7. Compile a small synthetic HLSL shader with DXC.
8. Render an offscreen triangle and read back pixels.
9. Export JSON hardware report.
10. Display results in the GUI.

**Exit gate:** repeatable WARP smoke tests pass; physical GPU tests are separately labeled.

### Phase 4 — IPC and runtime lifecycle

Tasks:

1. Define protocol version and message schema.
2. Implement named-pipe server/client with current-user ACL.
3. Implement handshake and version mismatch handling.
4. Add runtime lifecycle state machine.
5. Add structured event streaming.
6. Add graceful stop, timeout and forced process termination fallback.
7. Add crash detection and log correlation.
8. Create synthetic runtime that emits staged events.
9. Connect the UI to the synthetic runtime.

**Exit gate:** UI can launch, monitor and stop the synthetic runtime; runtime crash leaves UI alive.

### Phase 5 — Test module loader and platform service framework

Tasks:

1. Design a synthetic module format/fixture generated by the project.
2. Implement bounded header parsing and architecture checks.
3. Implement an explicit import registry.
4. Implement unresolved-import reporting.
5. Implement a small set of host service interfaces with project-owned test modules.
6. Implement memory, clock and synchronization tests.
7. Add thread-safety and unload tests.
8. Document supported and unsupported ABI behavior.

**Exit gate:** synthetic module runs through the loader and reports all imports deterministically.

### Phase 6 — Graphics compatibility foundation

Tasks:

1. Define backend-independent graphics abstractions.
2. Implement D3D12 device/resource/queue ownership.
3. Implement descriptors and resource-state tracking.
4. Implement barriers and fence synchronization.
5. Implement root-signature and PSO setup for synthetic workloads.
6. Implement draw/dispatch/indirect synthetic fixtures.
7. Add timestamp queries and per-operation tracing.
8. Add device-loss reporting and DRED capture.

**Exit gate:** synthetic graphics suite produces the expected image and detailed timing reports on WARP and at least one physical GPU.

### Phase 7 — Shader translator research prototype

Tasks:

1. Define shader input/output format and stable hash scheme.
2. Define shader IR and verifier.
3. Implement decoder for a very small, documented instruction subset.
4. Implement a translator for synthetic vertex/pixel/compute shaders.
5. Compile generated HLSL using DXC.
6. Validate the resulting DXIL.
7. Compare output pixels or compute buffers against CPU reference code.
8. Add explicit unsupported-instruction diagnostics.
9. Add versioned shader cache.
10. Add GUI compile queue and result view.

**Exit gate:** a small synthetic shader set passes deterministic output comparison on WARP and a physical GPU. Do not claim broad RDNA2 compatibility.

### Phase 8 — Resource/texture format framework

Tasks:

1. Create a generic bounded binary-reader utility.
2. Implement tests for short input, malformed offsets, integer overflow and oversized payloads.
3. Add generic texture decoder modules for licensed/public formats.
4. Add image inspection through generated synthetic textures.
5. Add local asset-provider interface.
6. Add file fingerprinting and read-only folder scan.
7. Ensure output and caches remain local.

**Exit gate:** readers pass malformed-input fuzz tests and never write into the selected source folder.

### Phase 9 — Runtime integration

Tasks:

1. Re-audit current VIctory/victory-fork/AnyXboxX findings against the implemented interfaces.
2. Create a mapping from external observed behavior to our interfaces.
3. Implement only approved compatibility services, one service at a time.
4. Add test fixtures for every new import/service.
5. Add graphics mappings only when equivalent synthetic tests exist.
6. Add a local read-only user-content provider.
7. Keep unknown imports fatal or explicitly unsupported; do not silently fake success.
8. Run tests after every integration step.

**Exit gate:** the runtime can initialize and process supported test modules and approved local data without undefined behavior.

### Phase 10 — Input, audio and UI integration

Tasks:

1. Add gamepad enumeration and hot-plug.
2. Add keyboard/mouse event translation.
3. Add dead-zone/configuration support only after tested.
4. Add a basic Windows audio backend.
5. Add audio diagnostics and device selection.
6. Show active input/audio devices in the GUI.
7. Add controller and audio synthetic tests.

**Exit gate:** the synthetic runtime receives deterministic input and produces an audio test signal without UI blocking.

### Phase 11 — Performance optimization

Tasks:

1. Capture baseline timing for each synthetic workload.
2. Add pipeline-cache prewarming and validation.
3. Add async shader compilation with correct synchronization.
4. Measure and reduce repeated buffer copies.
5. Introduce upload ring buffers.
6. Add resource lifetime and GPU memory-budget diagnostics.
7. Prototype submission overlap behind a feature flag.
8. Prototype dirty-page/mirror caching behind a feature flag.
9. Prototype indirect-argument pre-pass only with a correctness proof.
10. Compare before/after output hashes and performance distributions.
11. Remove any optimization that causes visual or data divergence.

**Exit gate:** no enabled optimization changes verified synthetic output, and the performance report records both speed and stability effects.

### Phase 12 — Hardware validation and low-VRAM tuning

Tasks:

1. Validate on 8 GB VRAM class hardware.
2. Validate on one newer NVIDIA adapter.
3. Validate on AMD if available.
4. Validate WARP smoke tests on clean Windows installation.
5. Test shader cache cold and warm.
6. Test out-of-memory/back-pressure handling.
7. Test device removal and recovery reporting.
8. Publish exact test machine details and reproducible commands.

**Exit gate:** hardware support matrix is truthful and reproducible; unsupported features are visibly disabled.

### Phase 13 — Packaging and first public developer build

Tasks:

1. Build signed/versioned UI and runtime artifacts where signing is available.
2. Include only project binaries and licensed dependencies.
3. Include source, license and third-party notices.
4. Generate SHA-256 checksums for release artifacts.
5. Run a clean-machine install/uninstall test.
6. Verify logs do not expose full personal paths in shared reports.
7. Update README with exact evidence status.
8. Publish known limitations and test hardware.

**Exit gate:** reproducible developer build with complete notices, no commercial game content and no unsupported playability claims.

---

## 15. Coding-agent operating rules

Add these to the project's `AGENTS.md` and follow them in every session.

1. At session start, read `AGENTS.md`, `docs/STATUS_EVIDENCE.md`, `docs/FEATURE_SELECTION.md`, `docs/LICENSE_MATRIX.md`, and the relevant subsystem design.
2. Check the current git status. Never overwrite unrelated user changes.
3. Work on one milestone and one focused subsystem at a time.
4. Before editing a subsystem, inspect its current code and tests.
5. For every bug, first add a failing test or a reproducible synthetic case.
6. Keep commits small and conventional (`feat:`, `fix:`, `test:`, `docs:`, `build:`, `refactor:`).
7. Do not make large mechanical refactors while implementing a new feature.
8. Do not add placeholder implementations that return success for unsupported operations.
9. Do not fake progress, FPS, compatibility, or successful rendering in production mode.
10. Every new runtime behavior needs tests and a status-evidence update.
11. Every source-derived change needs a source-provenance entry and a license check.
12. Run formatting, build, unit tests and relevant integration tests before committing.
13. Keep the UI responsive; move blocking work to worker threads/processes.
14. Use RAII for handles, COM objects, allocations, threads, locks and GPU resources.
15. Prefer explicit error types and structured logs over silent catch-all handlers.
16. Add bounds checks to every parser and binary-data path.
17. Never run unknown external binaries or repository scripts without inspecting them first.
18. Do not retrieve, decrypt, unpack, distribute or commit game data or keys.
19. Do not patch the target executable, disable its watchdog, defeat access controls, or interfere with online protections.
20. At the end of each task, report: changed files, behavior added, tests run, tests not run, remaining risks, and next smallest task.

### 15.1 Agent task format

Every task must have:

- Objective.
- Files allowed to change.
- Prerequisites.
- Exact implementation steps.
- Tests to add/run.
- Acceptance criteria.
- Out-of-scope changes.
- Provenance/license notes if code is based on an external project.

Do not start a second large subsystem until the first one builds and passes its acceptance tests.

---

## 16. First actions the agent must take

Start here; do not jump directly into shader translation or a full-game launch attempt.

1. Inspect the current working directory and preserve existing user files.
2. Create a new branch named `feat/cpp-runtime-foundation` if this is an existing Git repository; otherwise initialize the repository only after confirming the directory is intended for this project.
3. Create `docs/` and complete the Phase 0 audit documents.
4. Audit the latest public commits and licenses of the repositories in Section 3.
5. Select and document the initial source reuse policy. Default: MIT-compatible C++ testing/tooling from Xbox2Windows may be reused with notices; GPL-3-compatible runtime work may be reused only if the new runtime accepts GPL-3 obligations; XCL GPL-2-only code is reference-only unless explicit permission is obtained.
6. Create the CMake/C++ skeleton and Qt shell.
7. Add the mock dashboard and settings persistence.
8. Add a synthetic GPU probe and WARP offscreen render test.
9. Add named-pipe IPC and a fake runtime event source.
10. Update all documentation and provide a milestone report.

**Do not begin by cloning every repository and asking an LLM to merge all files.** That approach risks license contamination, duplicate subsystems, incompatible APIs, untested patches, and an unmaintainable codebase.

---

## 17. Acceptance checklist for the overall project

### Build and structure

- [ ] CMake clean build on Windows x64.
- [ ] C++20 enforced in CMake.
- [ ] Qt UI and runtime host build as separate executables.
- [ ] vcpkg dependencies pinned.
- [ ] CI builds Debug and Release.
- [ ] Documentation and provenance files present.

### UI

- [ ] Dark desktop UI with Dashboard, Setup, Graphics, Shader Manager, Diagnostics, Logs, Controls, Benchmark and About pages.
- [ ] Mock mode works without runtime or game data.
- [ ] UI remains responsive during long background work.
- [ ] Runtime crash does not crash the UI.
- [ ] Settings persist and can be reset.

### Runtime

- [ ] Runtime lifecycle state machine tested.
- [ ] Named-pipe IPC versioned and secured to current user.
- [ ] Synthetic module loader works.
- [ ] Unsupported imports are reported correctly.
- [ ] Platform-service tests pass.
- [ ] Runtime threads and GPU resources shut down cleanly.

### Graphics and shaders

- [ ] D3D12/WARP device tests pass.
- [ ] DXC compile/validation test passes.
- [ ] Synthetic shader pixel/buffer comparisons pass.
- [ ] Resource and barrier tests pass.
- [ ] Pipeline cache is versioned and corruption-safe.
- [ ] DRED/device-removal reporting works where supported.

### Data and safety

- [ ] Read-only local content validation works.
- [ ] No package downloader/decryptor/unpacker exists.
- [ ] No game data, executable, keys, assets, or save data is shipped.
- [ ] Logs can be redacted before sharing.
- [ ] No unsupported online, anti-cheat or authentication bypasses.

### Quality

- [ ] Every feature status has an evidence level.
- [ ] 8 GB VRAM behavior is tested or marked unverified.
- [ ] All external source reuse is recorded.
- [ ] License and third-party notices are complete.
- [ ] No fabricated FPS or playable claims.

---

## 18. Research references and evidence notes

These are the primary pages reviewed for this document. The agent must re-check live repository state before acting; this document is a dated research snapshot, not a permanent guarantee.

### GitHub repositories

- [VIctory — Marvisak](https://github.com/Marvisak/VIctory)
- [victory-fork — fxceless](https://github.com/fxceless/victory-fork)
- [AnyXboxX — LooperSalty](https://github.com/LooperSalty/AnyXboxX)
- [CGBJ-VIctory — JordanFromIT](https://github.com/JordanFromIT/CGBJ-VIctory) — search-indexed README; repository page was not retrievable in this check, so verify availability before use.
- [Flamingo — realblastex](https://github.com/realblastex/flamingo)
- [VItoriaBr — uiliannn](https://github.com/uiliannn/VItoriaBr)
- [Five Star Recomp — ThatRecompDev](https://github.com/ThatRecompDev/five-star-recomp)
- [Xbox2Windows — R0Y00](https://github.com/R0Y00/Xbox2Windows)
- [XCL — xcl-dev](https://github.com/xcl-dev/xcl)
- [ScarlettPort — ZigZagBoi1](https://github.com/ZigZagBoi1/ScarlettPort)
- [Project ReCity — pixl27](https://github.com/pixl27/ProjectReCity)

### Trackers and community reports

- [GTA PC Port Tracker](https://gtapcport.org/) — project stage and activity snapshot.
- [UFD Tech GTA VI PC Port Tracker](https://tech.ufdmedia.com/trackers/gta-vi-pc-port/) — projects, milestones and evidence-level distinctions.
- [GTA VI PC Race tracker](https://vuyp.github.io/gta6-pc-race/) — community-maintained stage comparison; independently verify every claim.
- [Reddit: GTA 6 PC port status discussion](https://www.reddit.com/r/FitGirlRepack/comments/1y4dbfo/gta_6_pc_port_status_24_hours_after_exe_drop/) — discussion of current projects and technical distinctions; comments are not authoritative technical evidence.

### Key facts used in this plan

- The current leading runtime/compatibility projects are not all C++; the more advanced runtime branches are predominantly Rust-based.
- Xbox2Windows is a genuine C++/CMake/D3D12 research project, but its own README does not claim verified full-game startup.
- VIctory-derived projects use GPL-3.0-or-later licensing; XCL is GPL-2.0-only; Xbox2Windows is MIT. Do not merge incompatible source without permission and a license review.
- ScarlettPort documents separate XCL and VIctory runtime trees with their own licenses and a Rust/egui launcher; this is an architectural reference, not a license shortcut.
- Some trackers report limited controllable/“parts playable” runs, while other trackers state that no project is fully playable throughout. Treat such stages as self-reported and define our own evidence levels.
- An x86-64 CPU translation is not the main shortcut: native CPU execution may be possible, while platform services, graphics/shader translation and resource behavior remain the principal engineering work.

---

## Final instruction to the coding agent

Build a **real C++ project**, not a folder of copied repositories. Start with the legal/provenance audit, build system, Qt mock GUI, D3D12/WARP synthetic tests, and runtime IPC. Then add the compatibility runtime in small, measurable modules. Use the strongest external work as carefully audited reference material and reuse only code whose license permits the intended result. Keep all commercial content outside the repository and outside public releases.

Your first deliverable is a clean, working C++/Qt application and a verified test foundation. The compatibility runtime comes next, one tested subsystem at a time.
