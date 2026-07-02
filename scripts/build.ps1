param(
  [switch]$NativeOnly,
  [switch]$RustOnly,
  [switch]$FrontendOnly
)

$ErrorActionPreference = "Stop"
$RepoRoot = Split-Path -Parent $PSScriptRoot

if (-not $NativeOnly -and -not $FrontendOnly) {
  Push-Location $RepoRoot
  cargo check --workspace
  Pop-Location
}

if (-not $NativeOnly -and -not $RustOnly) {
  Push-Location (Join-Path $RepoRoot "apps/desktop")
  npm install
  npm run build
  Pop-Location
}

if (-not $RustOnly -and -not $FrontendOnly) {
  $BuildDir = Join-Path $RepoRoot "native/windows-virtual-camera/build"
  cmake -S (Join-Path $RepoRoot "native/windows-virtual-camera") -B $BuildDir
  cmake --build $BuildDir --config Debug
}

