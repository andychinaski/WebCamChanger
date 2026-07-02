# First Iteration Implementation Plan

## System Requirements From The Spec

- Windows 10 and Windows 11.
- Virtual webcam output named `Chinaski Virtual Camera`.
- Tauri v2 desktop shell with React, TypeScript, and Tailwind CSS.
- Rust backend behind Tauri commands.
- Native C++ Windows virtual camera module.
- Windows 11 virtual camera registration through Media Foundation `MFCreateVirtualCamera`.
- Windows 10 virtual camera registration through a DirectShow capture source filter.
- Video-only output, no audio.
- Target frame format: 1280x720, 30 FPS, RGB32 or NV12.

## MVP Scope

Full MVP from the spec includes virtual camera registration, physical camera source, image source, video source, library search, preview, fallback, resolution/FPS settings, and looped video.

The first spike narrows that to:

- project scaffold;
- minimal Tauri UI;
- Rust lifecycle commands;
- native Windows virtual camera module skeleton;
- explicit path to register `Chinaski Virtual Camera`;
- native CLI helper `chinaski-vcamctl.exe` for support/register/unregister diagnostics;
- fallback/test frame placeholder;
- manual verification docs for Windows and Zoom.
- standalone Tauri desktop executable/MSI packaging for the shell.

## Technical Risks

- Zoom may not list the camera unless backend registration and categories are exactly right.
- COM registration and virtual camera install/remove flow may require elevation.
- Windows camera privacy settings can hide or block the device.
- A visible device is not enough; Zoom must also open the Custom Media Source and receive stable frames.
- Frame pacing and timestamps can break display even after registration succeeds.

## Acceptance Criteria For The Spike

- Repository structure exists.
- README and architecture/testing/spike docs exist.
- Desktop shell builds.
- Desktop app packages into a Windows `.exe` and `.msi`.
- UI exposes virtual camera lifecycle actions.
- Rust backend returns diagnostics and errors to the UI.
- Native module boundary exists for backend-specific virtual camera registration.
- Native helper reports real Windows virtual camera backend support status.
- The implementation does not pretend that real virtual camera output is complete before Zoom validates it.

## 5-7 Step Plan

1. Create monorepo layout with `apps`, `crates`, `native`, `docs`, `scripts`, and `assets`.
2. Add Tauri v2, React, TypeScript, Tailwind shell with lifecycle controls and diagnostics.
3. Add Rust crates for app core, diagnostics, media pipeline metadata, and virtual camera bridge.
4. Add C++ native module skeleton with exported backend detection and registration functions.
5. Add scripts for build and manual camera checks.
6. Add desktop packaging script for the Tauri shell.
7. Document architecture, spike notes, testing, limitations, and next tasks.

## Explicit Placeholders

- Real COM Custom Media Source for Windows 11: `planned`.
- Real DirectShow capture source filter for Windows 10: `planned`.
- `MFCreateVirtualCamera` registration: `planned`.
- Frame transport from Rust to C++: `planned`.
- Actual fallback frame delivery to Zoom: `planned`.
- GStreamer/FFmpeg pipeline: `planned`.
- SQLite library and search: `planned`.
- Physical camera/image/video sources: `planned`.
- Installer/uninstaller: `planned`.
- Desktop app MSI: `spike`.
- Virtual camera native installer/uninstaller: `planned`.
