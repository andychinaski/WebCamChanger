use diagnostics::{CameraStatus, DiagnosticsLog};
pub use diagnostics::DiagnosticsSnapshot;
use media_pipeline::TestFrame;
use virtual_camera_bridge::{VirtualCameraBridge, CAMERA_NAME};

pub struct AppCore {
    diagnostics: DiagnosticsLog,
    bridge: VirtualCameraBridge,
    test_frame: Option<TestFrame>,
}

impl AppCore {
    pub fn new() -> Self {
        Self {
            diagnostics: DiagnosticsLog::new(CAMERA_NAME),
            bridge: VirtualCameraBridge::new(CAMERA_NAME),
            test_frame: None,
        }
    }

    pub fn register_virtual_camera(&mut self) -> Result<DiagnosticsSnapshot, String> {
        match self.bridge.register() {
            Ok(()) => {
                self.diagnostics.set_status(CameraStatus::Registered);
                self.diagnostics.info(format!(
                    "Registered '{}' as a Windows virtual camera.",
                    self.bridge.camera_name()
                ));
                Ok(self.diagnostics())
            }
            Err(error) => {
                self.diagnostics.error(format!("Registration failed: {error}"));
                Err(error.to_string())
            }
        }
    }

    pub fn unregister_virtual_camera(&mut self) -> Result<DiagnosticsSnapshot, String> {
        match self.bridge.unregister() {
            Ok(()) => {
                self.diagnostics.set_output_active(false);
                self.diagnostics.set_status(CameraStatus::Unregistered);
                self.diagnostics.info("Unregistered virtual camera.");
                Ok(self.diagnostics())
            }
            Err(error) => {
                self.diagnostics.error(format!("Unregister failed: {error}"));
                Err(error.to_string())
            }
        }
    }

    pub fn start_output(&mut self) -> Result<DiagnosticsSnapshot, String> {
        self.test_frame = Some(TestFrame::fallback_color_frame());

        match self.bridge.start_output() {
            Ok(()) => {
                self.diagnostics.set_status(CameraStatus::Streaming);
                self.diagnostics.set_output_active(true);
                self.diagnostics.set_test_frame_ready(true);
                self.diagnostics
                    .info("Started SPIKE output with fallback color frame metadata.");
                Ok(self.diagnostics())
            }
            Err(error) => {
                self.diagnostics.error(format!("Start output failed: {error}"));
                Err(error.to_string())
            }
        }
    }

    pub fn stop_output(&mut self) -> Result<DiagnosticsSnapshot, String> {
        match self.bridge.stop_output() {
            Ok(()) => {
                self.diagnostics.set_status(CameraStatus::Stopped);
                self.diagnostics.set_output_active(false);
                self.diagnostics.info("Stopped virtual camera output.");
                Ok(self.diagnostics())
            }
            Err(error) => {
                self.diagnostics.error(format!("Stop output failed: {error}"));
                Err(error.to_string())
            }
        }
    }

    pub fn diagnostics(&self) -> DiagnosticsSnapshot {
        self.diagnostics.snapshot()
    }
}

impl Default for AppCore {
    fn default() -> Self {
        Self::new()
    }
}
