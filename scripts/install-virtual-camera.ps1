$ErrorActionPreference = "Stop"

Write-Host "Chinaski Virtual Camera registration spike"
Write-Host "This script is intentionally not a fake installer yet."
Write-Host ""
Write-Host "Required next native work:"
Write-Host "1. Implement COM Custom Media Source DLL."
Write-Host "2. Register the COM class."
Write-Host "3. Call MFCreateVirtualCamera with name 'Chinaski Virtual Camera'."
Write-Host "4. Persist/remove the IMFVirtualCamera registration during install/uninstall."
Write-Host ""
Write-Host "Run from an elevated PowerShell once the native module is implemented."
exit 2

