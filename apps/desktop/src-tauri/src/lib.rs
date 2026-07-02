use std::sync::Mutex;

use app_core::{AppCore, DiagnosticsSnapshot};
use tauri::{Manager, State};

struct AppState {
    core: Mutex<AppCore>,
}

#[tauri::command]
fn register_virtual_camera(state: State<'_, AppState>) -> Result<DiagnosticsSnapshot, String> {
    state.core.lock().map_err(|error| error.to_string())?.register_virtual_camera()
}

#[tauri::command]
fn unregister_virtual_camera(state: State<'_, AppState>) -> Result<DiagnosticsSnapshot, String> {
    state.core.lock().map_err(|error| error.to_string())?.unregister_virtual_camera()
}

#[tauri::command]
fn start_output(state: State<'_, AppState>) -> Result<DiagnosticsSnapshot, String> {
    state.core.lock().map_err(|error| error.to_string())?.start_output()
}

#[tauri::command]
fn stop_output(state: State<'_, AppState>) -> Result<DiagnosticsSnapshot, String> {
    state.core.lock().map_err(|error| error.to_string())?.stop_output()
}

#[tauri::command]
fn show_diagnostics(state: State<'_, AppState>) -> Result<DiagnosticsSnapshot, String> {
    Ok(state.core.lock().map_err(|error| error.to_string())?.diagnostics())
}

pub fn run() {
    tauri::Builder::default()
        .setup(|app| {
            app.manage(AppState {
                core: Mutex::new(AppCore::new()),
            });
            Ok(())
        })
        .invoke_handler(tauri::generate_handler![
            register_virtual_camera,
            unregister_virtual_camera,
            start_output,
            stop_output,
            show_diagnostics
        ])
        .run(tauri::generate_context!())
        .expect("failed to run Tauri application");
}

