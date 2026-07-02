use std::{
    path::PathBuf,
    process::Command,
};

pub const CAMERA_NAME: &str = "Chinaski Virtual Camera";

#[derive(Debug, thiserror::Error)]
pub enum VirtualCameraError {
    #[error("Windows is required for this spike")]
    UnsupportedPlatform,
    #[error("native helper was not found; build native/windows-virtual-camera first. Checked: {0:?}")]
    NativeHelperMissing(Vec<PathBuf>),
    #[error("native helper command failed: {command}; code: {code:?}; stdout: {stdout}; stderr: {stderr}")]
    NativeCommandFailed {
        command: String,
        code: Option<i32>,
        stdout: String,
        stderr: String,
    },
    #[error("failed to start native helper {path:?}: {source}")]
    NativeHelperLaunch {
        path: PathBuf,
        source: std::io::Error,
    },
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
        self.run_helper("status")?;
        self.run_helper("register")
    }

    pub fn unregister(&self) -> Result<()> {
        ensure_windows()?;
        self.run_helper("unregister")
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
        PathBuf::from("native/windows-virtual-camera/build/Debug/ChinaskiVirtualCamera.dll")
    }

    pub fn expected_helper_path(&self) -> PathBuf {
        PathBuf::from("native/windows-virtual-camera/build-release/chinaski-vcamctl.exe")
    }

    fn run_helper(&self, action: &str) -> Result<()> {
        let helper = find_helper()?;
        let output = Command::new(&helper)
            .arg(action)
            .output()
            .map_err(|source| VirtualCameraError::NativeHelperLaunch {
                path: helper.clone(),
                source,
            })?;

        if output.status.success() {
            return Ok(());
        }

        Err(VirtualCameraError::NativeCommandFailed {
            command: format!("{} {}", helper.display(), action),
            code: output.status.code(),
            stdout: String::from_utf8_lossy(&output.stdout).trim().to_string(),
            stderr: String::from_utf8_lossy(&output.stderr).trim().to_string(),
        })
    }
}

fn find_helper() -> Result<PathBuf> {
    let mut candidates = Vec::new();

    if let Ok(current_exe) = std::env::current_exe() {
        if let Some(dir) = current_exe.parent() {
            candidates.push(dir.join("chinaski-vcamctl.exe"));
            candidates.push(dir.join("native").join("chinaski-vcamctl.exe"));
        }
    }

    if let Ok(manifest_dir) = std::env::var("CARGO_MANIFEST_DIR") {
        let src_tauri_dir = PathBuf::from(manifest_dir);
        if let Some(repo_root) = src_tauri_dir
            .parent()
            .and_then(|desktop| desktop.parent())
            .and_then(|apps| apps.parent())
        {
            candidates.push(
                repo_root
                    .join("native/windows-virtual-camera/build-release/chinaski-vcamctl.exe"),
            );
            candidates.push(
                repo_root
                    .join("native/windows-virtual-camera/build/Debug/chinaski-vcamctl.exe"),
            );
            candidates.push(
                repo_root
                    .join("native/windows-virtual-camera/build/Release/chinaski-vcamctl.exe"),
            );
        }
    }

    candidates.push(PathBuf::from(
        "native/windows-virtual-camera/build-release/chinaski-vcamctl.exe",
    ));
    candidates.push(PathBuf::from(
        "native/windows-virtual-camera/build/Debug/chinaski-vcamctl.exe",
    ));
    candidates.push(PathBuf::from(
        "native/windows-virtual-camera/build/Release/chinaski-vcamctl.exe",
    ));

    for candidate in &candidates {
        if candidate.exists() {
            return Ok(candidate.clone());
        }
    }

    Err(VirtualCameraError::NativeHelperMissing(candidates))
}

fn ensure_windows() -> Result<()> {
    if cfg!(target_os = "windows") {
        Ok(())
    } else {
        Err(VirtualCameraError::UnsupportedPlatform)
    }
}
