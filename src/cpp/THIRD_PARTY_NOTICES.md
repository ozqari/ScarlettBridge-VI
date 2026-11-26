# THIRD_PARTY_NOTICES.md

No third-party source code is vendored in this tree.

Build-time system components (not distributed): MSVC toolchain, Windows SDK
(DXGI/D3D12/D3DCompiler), Qt 6 Widgets (system install, UI target only),
CMake, Ninja, vcpkg (manifest currently declares zero dependencies).

Upstream reference projects (behavioral reference only, nothing copied —
see `../docs/REPO_AUDIT.md`, `../docs/LICENSE_MATRIX.md`,
`../docs/SOURCE_PROVENANCE.csv`): VIctory, victory-fork, AnyXboxX, Flamingo,
VItoriaBr, five-star-recomp (all GPL-3.0-or-later); Xbox2Windows (MIT);
XCL (GPL-2.0-only, reference-only); ScarlettPort (mixed per-part);
ProjectReCity (GPL-3.0-or-later).
