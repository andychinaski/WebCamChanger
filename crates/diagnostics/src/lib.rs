use serde::Serialize;

#[derive(Debug, Clone, Copy, PartialEq, Eq, Serialize)]
#[serde(rename_all = "snake_case")]
pub enum CameraStatus {
    Unregistered,
    Registered,
    Streaming,
    Stopped,
    Error,
}

#[derive(Debug, Clone, Serialize)]
pub struct DiagnosticsSnapshot {
    pub camera_name: String,
    pub status: CameraStatus,
    pub output_active: bool,
    pub test_frame_ready: bool,
    pub events: Vec<String>,
    pub last_error: Option<String>,
}

#[derive(Debug, Clone)]
pub struct DiagnosticsLog {
    camera_name: String,
    status: CameraStatus,
    output_active: bool,
    test_frame_ready: bool,
    events: Vec<String>,
    last_error: Option<String>,
}

impl DiagnosticsLog {
    pub fn new(camera_name: impl Into<String>) -> Self {
        Self {
            camera_name: camera_name.into(),
            status: CameraStatus::Unregistered,
            output_active: false,
            test_frame_ready: false,
            events: vec!["Application core initialized.".to_string()],
            last_error: None,
        }
    }

    pub fn set_status(&mut self, status: CameraStatus) {
        self.status = status;
    }

    pub fn set_output_active(&mut self, active: bool) {
        self.output_active = active;
    }

    pub fn set_test_frame_ready(&mut self, ready: bool) {
        self.test_frame_ready = ready;
    }

    pub fn info(&mut self, event: impl Into<String>) {
        self.last_error = None;
        self.push_event(event.into());
    }

    pub fn error(&mut self, event: impl Into<String>) {
        let event = event.into();
        self.last_error = Some(event.clone());
        self.status = CameraStatus::Error;
        self.push_event(event);
    }

    pub fn snapshot(&self) -> DiagnosticsSnapshot {
        DiagnosticsSnapshot {
            camera_name: self.camera_name.clone(),
            status: self.status,
            output_active: self.output_active,
            test_frame_ready: self.test_frame_ready,
            events: self.events.clone(),
            last_error: self.last_error.clone(),
        }
    }

    fn push_event(&mut self, event: String) {
        self.events.insert(0, event);
        self.events.truncate(40);
    }
}

