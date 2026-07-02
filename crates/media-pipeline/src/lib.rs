pub const MVP_WIDTH: u32 = 1280;
pub const MVP_HEIGHT: u32 = 720;
pub const MVP_FPS: u32 = 30;

#[derive(Debug, Clone)]
pub struct TestFrame {
    pub width: u32,
    pub height: u32,
    pub fps: u32,
    pub format: PixelFormat,
}

#[derive(Debug, Clone, Copy)]
pub enum PixelFormat {
    Rgb32,
}

impl TestFrame {
    pub fn fallback_color_frame() -> Self {
        Self {
            width: MVP_WIDTH,
            height: MVP_HEIGHT,
            fps: MVP_FPS,
            format: PixelFormat::Rgb32,
        }
    }
}

