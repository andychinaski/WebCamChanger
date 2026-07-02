# Chinaski Virtual Camera

Chinaski Virtual Camera is a Windows desktop technical spike for a virtual webcam router. The product goal is to expose selected media sources as a normal webcam device named `Chinaski Virtual Camera` so Zoom, Discord, browsers, and similar apps can consume it.

This repository is currently focused on the riskiest part first: proving Windows 11 can see a Media Foundation virtual camera device. UI polish, media library, physical camera capture, image/video decoding, and SQLite are intentionally deferred.

## Supported Platform

- Windows 11 only for the MVP spike.
- Windows 10, macOS, and Linux are out of scope.
- Video only. Audio is out of scope.

## Prerequisites

- Windows 11.
- Visual Studio 2022 Build Tools with C++ desktop workload.
- Windows 11 SDK with Media Foundation headers and libraries.
- Rust stable toolchain.
- Node.js 20+ and npm.
- CMake 3.24+.
- Tauri v2 prerequisites, including WebView2 Runtime.

## Build Instructions

```powershell
.\scripts\build.ps1
```

Useful partial builds:

```powershell
.\scripts\build.ps1 -RustOnly
.\scripts\build.ps1 -FrontendOnly
.\scripts\build.ps1 -NativeOnly
```

## Package Desktop App

To build a standalone Windows desktop executable and MSI installer:

```powershell
.\scripts\package-desktop.ps1
```

Outputs:

```text
target\release\chinaski-desktop.exe
target\release\bundle\msi\Chinaski Virtual Camera_0.1.0_x64_en-US.msi
```

The MSI currently packages the Tauri desktop shell only. It does not install or register the Windows Media Foundation virtual camera device yet. That will require the future native COM media source and elevated registration/unregistration step.

## Run Instructions

```powershell
cd apps\desktop
npm install
npm run tauri:dev
```

The UI exposes the first lifecycle buttons:

- Register virtual camera
- Start output
- Stop output
- Unregister virtual camera
- Show diagnostics

## How To Test The Virtual Camera

The current spike has the Tauri UI, Rust command surface, diagnostics path, and native C++ module skeleton. The actual `MFCreateVirtualCamera` registration still needs the COM Custom Media Source and installer/elevated registration work.

Once native registration is implemented:

1. Build the native module.
2. Run the registration step from elevated PowerShell.
3. Run `.\scripts\check-virtual-camera.ps1`.
4. Open Windows Settings > Bluetooth & devices > Cameras and look for `Chinaski Virtual Camera`.
5. Open Zoom > Settings > Video and choose `Chinaski Virtual Camera`.
6. Start output in the desktop app and verify the fallback/test frame appears.

## Known Limitations

- The native Media Foundation virtual camera registration is a documented placeholder.
- The C++ module does not yet implement a COM Custom Media Source.
- The fallback frame is represented as Rust metadata and a UI preview placeholder; it is not yet delivered to Windows camera clients.
- No physical camera, image, or video source is implemented yet.
- No SQLite media library yet.
- The desktop MSI exists, but virtual camera driver/native registration install and uninstall are not implemented yet.

## Troubleshooting

- If Zoom does not show the camera, verify Windows camera privacy settings and restart Zoom after registration.
- If registration fails, run the installer/registration helper from elevated PowerShell.
- If Windows does not list the camera, verify that the native COM media source was registered and that `MFCreateVirtualCamera` completed successfully.
- If another app is using a camera source, capture may fail later when physical camera input is implemented.
- If `cargo check` fails on non-Windows hosts, verify that Windows-only native calls remain behind platform boundaries.

## Architecture

See [docs/architecture.md](docs/architecture.md).

The first-iteration analysis and plan are in [docs/implementation-plan.md](docs/implementation-plan.md).

## Next Tasks

- Implement and register a C++ COM Custom Media Source.
- Wire `MFCreateVirtualCamera` and persistent remove/unregister flow.
- Add a frame transport boundary between Rust and native code.
- Push a generated RGB32/NV12 test frame into the virtual camera.
- Add static image source after Zoom sees the test frame.
- Add video source and loop scheduling after static image output works.
