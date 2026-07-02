# Manual Testing

These tests are for Windows 10 and Windows 11.

## 1. Desktop App Launch

1. Run:

   ```powershell
   cd apps\desktop
   npm install
   npm run tauri:dev
   ```

2. Verify the app opens.
3. Click `Show diagnostics`.
4. Verify diagnostics show `Chinaski Virtual Camera`.

Expected current spike result: app launches if dependencies are installed. Registration calls the native helper and reports either platform support failure, `E_NOTIMPL`, or a future success result.

## Desktop Packaging

1. Run:

   ```powershell
   .\scripts\package-desktop.ps1
   ```

2. Verify these outputs exist:

   ```text
   target\release\chinaski-desktop.exe
   target\release\bundle\msi\Chinaski Virtual Camera_0.1.0_x64_en-US.msi
   ```

3. Install the MSI.
4. Launch `Chinaski Virtual Camera` from the Start menu.

Expected current spike result: the desktop shell launches. The MSI does not register the virtual camera device yet.

## 2. Virtual Camera Registration

1. Build the native module:

   ```powershell
   .\scripts\build.ps1 -NativeOnly
   ```

2. Run the installer/registration helper from elevated PowerShell.
3. Click `Register virtual camera` in the app.

Expected final result: diagnostics show registered state.

Expected current spike result: registration calls `chinaski-vcamctl.exe`. The support check should pass and print the selected backend:

- Windows 10: `using backend directshow`
- Windows 11: `using backend media-foundation`

On Windows 10, the actual register step now attempts DirectShow COM/filter registration. Expected current non-elevated result is `0x80070005`; from elevated PowerShell it should register the filter. On Windows 11, register still returns native `E_NOTIMPL` until the Media Foundation Custom Media Source exists.

## 3. Visibility In Windows

1. Run:

   ```powershell
   .\scripts\check-virtual-camera.ps1
   ```

2. Open Windows Settings > Bluetooth & devices > Cameras.
3. Look for `Chinaski Virtual Camera`.

Expected final result: the camera appears as a video capture device.

## 4. Visibility In Zoom

1. Restart Zoom after registration.
2. Open Zoom > Settings > Video.
3. Select `Chinaski Virtual Camera`.

Expected final result: Zoom lists the camera and can open it. On Windows 10, Zoom/other conferencing apps are the primary validation target for the DirectShow backend; the Windows Camera app may not enumerate every DirectShow virtual source.

## 5. Fallback/Test Frame

1. Register the virtual camera.
2. Click `Start output`.
3. Select `Chinaski Virtual Camera` in Zoom.

Expected final result: Zoom displays a generated color frame or fallback image at the target output format.

Expected current spike result: the UI marks test frame metadata as ready, but no frame reaches Zoom yet.

## 6. Stop Output

1. With output active, click `Stop output`.
2. Watch diagnostics.
3. Keep Zoom open.

Expected final result: output stops cleanly and Zoom does not crash. A final design decision is needed on whether clients should see frozen last frame, black frame, or unavailable camera.

## 7. Unregister Virtual Camera

1. Close Zoom.
2. Click `Unregister virtual camera` or run the uninstall helper.
3. Run `.\scripts\check-virtual-camera.ps1`.

Expected final result: `Chinaski Virtual Camera` is removed from Windows and Zoom no longer lists it after restart.

## Follow-Up Tests

- Switch from fallback to static image without restarting Zoom.
- Switch from static image to video without restarting Zoom.
- Loop video at stable FPS.
- Disconnect an active physical camera and verify fallback.
- Measure latency and frame pacing.
