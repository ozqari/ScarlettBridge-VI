#Requires -Version 5.1
<#
.SYNOPSIS
  Installs the ScarlettBridge automatic release workflow.

.DESCRIPTION
  Copies release.yml next to this script into .github/workflows/release.yml
  at the repository root. The workspace forbids dot-directories under src/,
  so the workflow is stored here (without the dot) and installed with this
  one-step script.

  Repository root is detected by walking up from this script until a
  directory containing src/cpp/CMakeLists.txt is found; pass -RepoRoot
  explicitly if detection fails.

.EXAMPLE
  powershell -ExecutionPolicy Bypass -File src/tools/github-release/enable.ps1
#>
[CmdletBinding()]
param(
  [string]$RepoRoot = ""
)

$ErrorActionPreference = "Stop"

if (-not $RepoRoot) {
  $dir = $PSScriptRoot
  while ($dir) {
    if (Test-Path (Join-Path $dir "src/cpp/CMakeLists.txt")) {
      $RepoRoot = $dir
      break
    }
    $parent = Split-Path $dir -Parent
    if (-not $parent -or $parent -eq $dir) { break }
    $dir = $parent
  }
}

if (-not $RepoRoot -or -not (Test-Path (Join-Path $RepoRoot "src/cpp/CMakeLists.txt"))) {
  Write-Error "repository root not found (no src/cpp/CMakeLists.txt above $PSScriptRoot). Pass -RepoRoot explicitly."
  exit 1
}

$destDir = Join-Path $RepoRoot ".github/workflows"
New-Item -ItemType Directory -Force -Path $destDir | Out-Null
Copy-Item (Join-Path $PSScriptRoot "release.yml") (Join-Path $destDir "release.yml") -Force
Write-Host "installed .github/workflows/release.yml (root: $RepoRoot)"
Write-Host "next: git add .github/workflows/release.yml; git commit; git tag v0.2.0; git push origin v0.2.0"
