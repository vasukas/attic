pub use std::time::Duration;
pub use glam::{Vec2, UVec2};
pub use glium::{self, uniform, Surface};

pub type GliumContext = Rc<glium::Context>;

pub trait Drawer {
    fn set_size(&mut self, size: UVec2);
    fn update(&mut self, dt: Duration); 
    fn render(&mut self, target: &mut glium::Frame);
}