# Spike Notes

## Chosen Virtual Camera Approach

The spike follows the Windows 11 Media Foundation virtual camera path:

1. C++ COM DLL implements a Custom Media Source.
2. Installer/elevated helper registers the COM class.
3. Registration code calls `MFCreateVirtualCamera`.
4. Windows exposes `Chinaski Virtual Camera` as a video capture device.
5. Zoom or another client opens that device and receives frames from the Custom Media Source.

This is the primary technical risk. A preview inside the Tauri window is not enough and is not treated as success.

## Windows APIs

Planned Windows APIs and concepts:

- `MFCreateVirtualCamera`
- `IMFVirtualCamera`
- Media Foundation Custom Media Source interfaces
- COM registration for the media source DLL
- Windows camera privacy/device enumeration paths

The native skeleton links Media Foundation libraries but does not yet call a working `MFCreateVirtualCamera` path.

## Current Limitations

- `RegisterChinaskiVirtualCamera` returns `E_NOTIMPL`.
- No COM Custom Media Source exists yet.
- No frame buffer or IPC path exists yet.
- No persistent install/uninstall state exists yet.
- The Rust bridge returns a clear `NativeRegistrationMissing` error for registration.

## Permissions

Registration may require elevated PowerShell or an installer because COM registration and system-level virtual camera registration can touch protected system locations. Camera privacy permissions can also block visibility or access after registration.

## How To Verify Registration

After implementing registration:

```powershell
.\scripts\install-virtual-camera.ps1
.\scripts\check-virtual-camera.ps1
```

Also verify manually:

1. Windows Settings > Bluetooth & devices > Cameras.
2. Device Manager under camera/imaging devices.
3. Windows Camera app if it allows device selection.
4. Zoom > Settings > Video.

Success means Zoom lists `Chinaski Virtual Camera` as a camera device. The next success criterion is that Zoom displays the test frame.

## Zoom-Specific Risks

- Zoom may cache camera device lists; restart Zoom after registration.
- Privacy settings can block camera access.
- Incorrect device category or source attributes can make the device invisible.
- A COM registration mismatch can make Windows see the device but fail to open it.
- Zoom may require stable timestamps and frame pacing before it displays a frame.

## First Spike Acceptance

This spike is not complete until a Windows 11 machine confirms:

- `Chinaski Virtual Camera` appears as a video capture device;
- Zoom lists it;
- a generated fallback/test frame appears in Zoom.

The current repository state reaches the project scaffold and command boundary, but the native MF implementation still needs focused work.

