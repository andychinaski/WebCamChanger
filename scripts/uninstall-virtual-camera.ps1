$ErrorActionPreference = "Stop"

$RepoRoot = Split-Path -Parent $PSScriptRoot
$Candidates = @(
  (Join-Path $RepoRoot "native/windows-virtual-camera/build-release/chinaski-vcamctl.exe"),
  (Join-Path $RepoRoot "native/windows-virtual-camera/build/Debug/chinaski-vcamctl.exe"),
  (Join-Path $RepoRoot "native/windows-virtual-camera/build/Release/chinaski-vcamctl.exe")
)

$Helper = $Candidates | Where-Object { Test-Path $_ } | Select-Object -First 1

if (-not $Helper) {
  Write-Error "chinaski-vcamctl.exe was not found. Run: .\scripts\build.ps1 -NativeOnly"
}

$principal = New-Object Security.Principal.WindowsPrincipal([Security.Principal.WindowsIdentity]::GetCurrent())
if (-not $principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)) {
  Write-Warning "Virtual camera unregister may require elevated PowerShell."
}

Write-Host "Unregistering Chinaski Virtual Camera..."
& $Helper unregister
exit $LASTEXITCODE
