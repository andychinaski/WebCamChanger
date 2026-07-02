# Spike Notes

## Chosen Virtual Camera Approach

The spike follows two Windows virtual camera paths:

1. Windows 11: C++ COM DLL implements a Media Foundation Custom Media Source.
2. Windows 11: installer/elevated helper registers the COM class and calls `MFCreateVirtualCamera`.
3. Windows 10: C++ COM DLL implements a DirectShow virtual capture source filter.
4. Windows 10: installer/elevated helper registers the filter under the video input device category.
5. Windows exposes `Chinaski Virtual Camera` as a video capture device.
6. Zoom or another client opens that device and receives frames from the selected native source/filter.

This is the primary technical risk. A preview inside the Tauri window is not enough and is not treated as success.

## Windows APIs

Planned Windows APIs and concepts:

- `MFCreateVirtualCamera`
- `IMFVirtualCamera`
- Media Foundation Custom Media Source interfaces
- DirectShow capture source filter interfaces
- COM registration for the media source/filter DLL
- Windows camera privacy/device enumeration paths

The native skeleton detects the target backend but does not yet register a working Media Foundation source or DirectShow filter.

## Current Limitations

- `CheckChinaskiVirtualCameraSupport` dynamically checks Media Foundation support on Windows 11 and DirectShow runtime support on Windows 10.
- `GetChinaskiVirtualCameraBackend` reports `media-foundation`, `directshow`, or `unsupported`.
- `RegisterChinaskiVirtualCamera` checks backend support first, then returns `E_NOTIMPL` until the native source/filter exists.
- No COM Custom Media Source or DirectShow source filter exists yet.
- No frame buffer or IPC path exists yet.
- No persistent install/uninstall state exists yet.
- The Rust bridge calls `chinaski-vcamctl.exe` and returns native stdout/stderr into Diagnostics.

## Permissions

Registration may require elevated PowerShell or an installer because COM registration and system-level virtual camera registration can touch protected system locations. Camera privacy permissions can also block visibility or access after registration.

## How To Verify Registration

After implementing registration:

```powershell
.\scripts\build.ps1 -NativeOnly
.\scripts\check-virtual-camera.ps1
.\scripts\install-virtual-camera.ps1
```

Also verify manually:

1. Windows Settings > Bluetooth & devices > Cameras.
2. Device Manager under camera/imaging devices.
3. Windows Camera app if it allows device selection.
4. Zoom > Settings > Video.

Success means Zoom lists `Chinaski Virtual Camera` as a camera device. The next success criterion is that Zoom displays the test frame.

On Windows 10, `check-virtual-camera.ps1` should report `using backend directshow`. On Windows 11, it should report `using backend media-foundation`.

## Zoom-Specific Risks

- Zoom may cache camera device lists; restart Zoom after registration.
- Privacy settings can block camera access.
- Incorrect device category or source attributes can make the device invisible.
- A COM registration mismatch can make Windows see the device but fail to open it.
- Zoom may require stable timestamps and frame pacing before it displays a frame.

## First Spike Acceptance

This spike is not complete until Windows 10 and Windows 11 confirm:

- `Chinaski Virtual Camera` appears as a video capture device;
- Zoom lists it;
- a generated fallback/test frame appears in Zoom.

The current repository state reaches the project scaffold, command boundary, and backend detection. Native Media Foundation and DirectShow registration/output still need focused work.
