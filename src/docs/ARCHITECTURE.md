# ARCHITECTURE.md — C++ plan + web-prototype mapping

## C++ target (brief section 6)

- scarlettbridge-ui.exe (Qt 6 Widgets): window, settings, lifecycle controller, diagnostics. Never owns GPU device.
- scarlettbridge-runtime.exe (C++20): loader, services, graphics device, shader cache, threads. Emits structured events. Headless-capable.
- scarlettbridge-cli.exe: same core libs + config schema, not a second implementation.
- Core libs: config, result, logging, events, tasking, versioning; ipc (protocol, named-pipe, messages); runtime (lifecycle, title-profile, memory, threads, file-access, module-loader, imports, diagnostics); platform/windows; graphics (abstraction, d3d12, shader-ir, translation, pipeline-cache, resource-tracking, presentation); media; formats; telemetry.
- IPC: local Windows named pipe, versioned, current-user ACL, capped sizes. No TCP control server.

## Web prototype mapping (this workspace)

- index.html approximates ui.exe shell: sidebar + topbar + 9 pages + settings store (localStorage, separate UI prefs vs runtime config).
- In-page RuntimeSim approximates runtime.exe: formal state machine Created, Validating, InitializingPlatform, InitializingGraphics, LoadingModules, Ready, Running, Stopping, Stopped, any to Failed. Fresh instance per Start; no orphaned timers or workers on stop.
- In-page IPC bus approximates named pipe: same message families + version handshake + size caps + fuzz tests (see IPC_PROTOCOL.md).
- WebGL2 probe approximates D3D12 smoke scope: adapter enum, device creation, resource alloc/copy/barrier timing, triangle readback. WARP, DXC, DRED stay DISABLED with reasons.
- Folder validator approximates asset provider: read-only, capped names/sizes/hashes, never writes source dir.

Interfaces (stable, no cycles): IRuntimeHost, IPlatformServices, IModuleLoader, IGraphicsBackend, IShaderTranslator, IAssetProvider, IAudioBackend, IInputBackend, IDiagnosticsSink. Each returns explicit result objects, no exception control-flow across boundaries. Web prototype implements mock versions with identical unsupported-call discipline (explicit unsupported, never silent success).
