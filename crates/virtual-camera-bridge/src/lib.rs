use std::path::PathBuf;

pub const CAMERA_NAME: &str = "Chinaski Virtual Camera";

#[derive(Debug, thiserror::Error)]
pub enum VirtualCameraError {
    #[error("Windows 11 is required for this spike")]
    UnsupportedPlatform,
    #[error("native Windows Media Foundation virtual camera registration is not implemented yet: {0}")]
    NativeRegistrationMissing(String),
}

pub type Result<T> = std::result::Result<T, VirtualCameraError>;

#[derive(Debug, Clone)]
pub struct VirtualCameraBridge {
    camera_name: String,
}

impl VirtualCameraBridge {
    pub fn new(camera_name: impl Into<String>) -> Self {
        Self {
            camera_name: camera_name.into(),
        }
    }

    pub fn camera_name(&self) -> &str {
        &self.camera_name
    }

    pub fn register(&self) -> Result<()> {
        ensure_windows()?;
        // SPIKE: Replace this with FFI into native/windows-virtual-camera once the COM
        // media source and installer registration path are implemented.
        Err(VirtualCameraError::NativeRegistrationMissing(
            "call native RegisterChinaskiVirtualCamera via installer/elevated helper".to_string(),
        ))
    }

    pub fn unregister(&self) -> Result<()> {
        ensure_windows()?;
        // SPIKE: This will delete the MF virtual camera instance created during register().
        Err(VirtualCameraError::NativeRegistrationMissing(
            "call native UnregisterChinaskiVirtualCamera via installer/elevated helper".to_string(),
        ))
    }

    pub fn start_output(&self) -> Result<()> {
        ensure_windows()?;
        // SPIKE: Frame transport is planned as a shared memory ring buffer or local IPC.
        Ok(())
    }

    pub fn stop_output(&self) -> Result<()> {
        ensure_windows()?;
        Ok(())
    }

    pub fn expected_native_module_path(&self) -> PathBuf {
        PathBuf::from("native/windows-virtual-camera/build/ChinaskiVirtualCamera.dll")
    }
}

fn ensure_windows() -> Result<()> {
    if cfg!(target_os = "windows") {
        Ok(())
    } else {
        Err(VirtualCameraError::UnsupportedPlatform)
    }
}

