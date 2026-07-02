# Architecture

Status: first technical spike.

Primary flow:

```text
UI
-> Tauri commands
-> Rust backend
-> Virtual Camera Bridge
-> Windows Virtual Camera Backend
   -> Windows 11: Media Foundation Custom Media Source
   -> Windows 10: DirectShow Capture Source Filter
-> Zoom / Discord / Browser
```

The UI must never call Media Foundation or DirectShow directly. React renders state and invokes Tauri commands. Rust owns application state, diagnostics, lifecycle orchestration, and eventually media pipeline state. The native C++ module owns the Windows camera boundary.

## Components

## UI Shell

Status: `spike`.

The current UI is intentionally minimal. It exposes lifecycle actions and diagnostics only:

- register virtual camera;
- start output;
- stop output;
- unregister virtual camera;
- show diagnostics.

The final dashboard, source cards, library views, and polished controls are `planned` until the virtual camera is visible in Zoom.

## Tauri Commands

Status: `spike`.

Commands live in `apps/desktop/src-tauri/src/lib.rs` and call `app-core`:

- `register_virtual_camera`
- `start_output`
- `stop_output`
- `unregister_virtual_camera`
- `show_diagnostics`

Each command returns a `DiagnosticsSnapshot` or an error string that the UI displays.

## Rust Backend

Status: `spike`.

`crates/app-core` owns lifecycle orchestration. It depends on:

- `crates/diagnostics` for event snapshots;
- `crates/media-pipeline` for test frame metadata;
- `crates/virtual-camera-bridge` for the native boundary.

SQLite, settings persistence, media library state, and source switching are `planned`.

## Source Manager

Status: `planned`.

Responsibilities:

- enumerate physical cameras;
- load image and video sources;
- select the active source;
- switch sources without restarting Zoom;
- emit source status and errors to diagnostics.

No source manager is implemented in this spike.

## Media Pipeline

Status: `planned`, with `spike` test-frame metadata.

Target output:

- 1280x720;
- 30 FPS;
- RGB32 or NV12;
- video only;
- stable frame pacing;
- fallback frame on source errors.

The current crate only defines the fallback/test frame metadata. Real decoding, frame normalization, scheduling, and GStreamer/FFmpeg integration are deferred.

## Virtual Camera Output

Status: `spike`.

`crates/virtual-camera-bridge` defines the Rust-facing lifecycle API and platform checks. `native/windows-virtual-camera` defines the C++ DLL/helper boundary and selects a backend.

Real work still required:

- implement a COM Custom Media Source for Windows 11;
- register the Windows 11 COM class and call `MFCreateVirtualCamera`;
- implement a DirectShow capture source filter for Windows 10;
- register/remove the selected backend;
- deliver frames from Rust to native code.

## Windows Native Camera Backends

Status: `planned`.

The C++ module currently contains a buildable registration skeleton and exported functions:

- `RegisterChinaskiVirtualCamera`
- `UnregisterChinaskiVirtualCamera`
- `CheckChinaskiVirtualCameraSupport`
- `GetChinaskiVirtualCameraBackend`

It detects Media Foundation on Windows 11 and DirectShow on Windows 10. Windows 10 now attempts DirectShow COM/filter registration. Windows 11 still returns `E_NOTIMPL` until the Media Foundation Custom Media Source and installation flow exist.

## Diagnostics

Status: `spike`.

Diagnostics show:

- camera name;
- lifecycle status;
- whether output is active;
- whether test-frame metadata is ready;
- recent events;
- last error.

Future diagnostics should add backend name, native HRESULTs, source health, FPS, pipeline latency, and privacy/access hints.

## Media Library

Status: `planned`.

The future library will use SQLite and store imported media, tags, search metadata, thumbnails, source stats, and last-used timestamps. It is not needed until the virtual camera registration and test frame are proven.
