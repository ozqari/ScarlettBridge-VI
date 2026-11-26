# BUILDING.md — web prototype (here) + Windows C++ (there)

## Here (no build)

No build step. Open the preview. All JS and CSS is inline in index.html so unsaved preview works without the src/ service worker.

## There (Windows 11 x64, VS2022 + CMake + Ninja + vcpkg + Qt6 + Windows SDK + DXC)

Presets: dev, release, ci. x64 only. C++20. /W4 + permissive- + EHsc + static analysis; warnings-as-errors in CI after baseline.

Manifest: vcpkg.json pinned baselines for Qt6 Widgets, spdlog+fmt, nlohmann/json, Catch2-or-gtest, optional ImGui. D3D12 and DXGI from Windows SDK; DXC from documented public distribution, version pinned.

CI: clean-checkout configure, restore pinned deps, Debug+Release build, CPU unit tests, WARP synthetic GPU tests where available, separate UI and runtime packaging, license and provenance check, format + analysis. Hardware GPU tests only on self-hosted runners with that GPU — never claim cloud-CI verified discrete-GPU features.

## Automatic releases (Windows x64)

The release pipeline is stored at `src/tools/github-release/release.yml`
(`src/` forbids dot-directories, so it cannot live at `.github/` in the
workspace). Enable it once at the GitHub repo root (the directory containing
`src/cpp/CMakeLists.txt`):

```bat
powershell -ExecutionPolicy Bypass -File src/tools/github-release/enable.ps1
git add .github/workflows/release.yml
```

Then: `git tag v0.2.0 && git push origin v0.2.0` builds, tests, packages and
publishes a GitHub Release (`-rc1`-style tags become pre-releases); Actions →
`release` → Run workflow dry-runs the same build without publishing. The
workflow runs `ctest --preset ci` before packaging (a red suite ships
nothing), scans the package for forbidden content (no game files, keys or
SDK material), and attaches SHA256 checksums plus `build-info.txt`. Qt is
best-effort: the UI binary ships only when Qt is found. No successful run
has been observed yet — record the first green log URL in
`STATUS_EVIDENCE.md` before treating any release as verified.
