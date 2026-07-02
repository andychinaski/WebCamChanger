$ErrorActionPreference = "Stop"

$RepoRoot = Split-Path -Parent $PSScriptRoot
$DesktopRoot = Join-Path $RepoRoot "apps/desktop"
$CargoBin = Join-Path $env:USERPROFILE ".cargo/bin"

if (Test-Path $CargoBin) {
  $env:Path = "$CargoBin;$env:Path"
}

Push-Location $DesktopRoot
npm install
npm run tauri:build
Pop-Location

Write-Host ""
Write-Host "Desktop executable:"
Write-Host (Join-Path $RepoRoot "target/release/chinaski-desktop.exe")
Write-Host ""
Write-Host "MSI bundle:"
Write-Host (Join-Path $RepoRoot "target/release/bundle/msi/Chinaski Virtual Camera_0.1.0_x64_en-US.msi")

