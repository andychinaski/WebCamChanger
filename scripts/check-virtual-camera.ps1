$CameraName = "Chinaski Virtual Camera"
$RepoRoot = Split-Path -Parent $PSScriptRoot
$Candidates = @(
  (Join-Path $RepoRoot "native/windows-virtual-camera/build-release/chinaski-vcamctl.exe"),
  (Join-Path $RepoRoot "native/windows-virtual-camera/build/Debug/chinaski-vcamctl.exe"),
  (Join-Path $RepoRoot "native/windows-virtual-camera/build/Release/chinaski-vcamctl.exe")
)

$Helper = $Candidates | Where-Object { Test-Path $_ } | Select-Object -First 1

if ($Helper) {
  Write-Host "Checking Windows virtual camera backend support..."
  & $Helper status
  Write-Host ""
} else {
  Write-Warning "chinaski-vcamctl.exe was not found. Run: .\scripts\build.ps1 -NativeOnly"
}

Write-Host "Checking camera devices for '$CameraName'..."
Get-PnpDevice -Class Camera -ErrorAction SilentlyContinue | Where-Object {
  $_.FriendlyName -like "*$CameraName*"
} | Format-Table -AutoSize

Write-Host ""
Write-Host "If nothing is listed, also check Windows Settings > Bluetooth & devices > Cameras and Zoom camera settings."
