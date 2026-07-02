$CameraName = "Chinaski Virtual Camera"

Write-Host "Checking camera devices for '$CameraName'..."
Get-PnpDevice -Class Camera -ErrorAction SilentlyContinue | Where-Object {
  $_.FriendlyName -like "*$CameraName*"
} | Format-Table -AutoSize

Write-Host ""
Write-Host "If nothing is listed, also check Windows Settings > Bluetooth & devices > Cameras and Zoom camera settings."

