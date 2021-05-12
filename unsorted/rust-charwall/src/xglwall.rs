#![allow(non_upper_case_globals)]
#![allow(non_camel_case_types)]
#![allow(non_snake_case)]
#![allow(dead_code)]

use glam::{IVec2, UVec2};
use glium;

include!(concat!(env!("OUT_DIR"), "/bindings.rs"));

#[derive(Default)]
pub struct XglwallInit {
    pub title: String,
    pub enable_input: bool,
}

pub struct KeyEvent {
    pub char: Option<char>,
    pub keysym: i32,     // X11 KeySym
    pub keysym_mod: i32, // KeySym with modifiers applied
}

pub enum MouseButton {
    None, // MouseMotion event
    Left,
    Middle,
    Right,
    WheelUp,
    WheelDown,
    Unknown(i32),
}

pub struct MouseEvent {
    pub button: MouseButton,
    pub pos: IVec2,
}

pub enum Event {
    Destroy,
    KeyPress(KeyEvent),
    KeyRelease(KeyEvent),
    MousePress(MouseEvent),
    MouseRelease(MouseEvent),
    MouseMotion(MouseEvent),
    Resize(UVec2),
}

pub struct Xglwall {
    size: UVec2,
}
impl Xglwall {
    pub fn init(init: XglwallInit) -> Option<Self> {
        let title = std::ffi::CString::new(&init.title[..]).unwrap();
        unsafe {
            let init = xglwall_init_params {
                title: if init.title.is_empty() {
                    std::ptr::null()
                }
                else {
                    title.as_ptr() as *const std::os::raw::c_char
                },
                log: None,
                log_userdata: std::ptr::null_mut(),
                display_name: std::ptr::null(),
                enable_input: if init.enable_input { 1 } else { 0 },
                opengl: xglwall_params_opengl_default(),
            };
            let mut init_out = std::mem::MaybeUninit::uninit().assume_init();
            match xglwall_init(&init, &mut init_out) {
                0 => None,
                _ => Some(Self {
                    size: UVec2::new(init_out.width as u32, init_out.height as u32),
                }),
            }
        }
    }
    pub fn poll_event(&mut self) -> Option<Event> {
        let mut ty = xglwall_event_type::MAX;
        unsafe {
            let mut ev = std::mem::MaybeUninit::uninit().assume_init();
            if xglwall_poll_event(&mut ty, &mut ev) != 0 {
                match ty {
                    xglwall_event_type_xglwall_event_destroy => Some(Event::Destroy),
                    xglwall_event_type_xglwall_event_key_press => {
                        Some(Event::KeyPress(fill_key_event(ev)))
                    }
                    xglwall_event_type_xglwall_event_key_release => {
                        Some(Event::KeyRelease(fill_key_event(ev)))
                    }
                    xglwall_event_type_xglwall_event_mouse_press => {
                        Some(Event::MousePress(fill_mouse_event(ev)))
                    }
                    xglwall_event_type_xglwall_event_mouse_release => {
                        Some(Event::MouseRelease(fill_mouse_event(ev)))
                    }
                    xglwall_event_type_xglwall_event_mouse_move => {
                        Some(Event::MouseMotion(fill_mouse_event(ev)))
                    }
                    xglwall_event_type_xglwall_event_window_resize => Some(Event::Resize(
                        UVec2::new(ev.resize.width as u32, ev.resize.height as u32),
                    )),
                    _ => None,
                }
            }
            else {
                None
            }
        }
    }
    pub fn render(&self) {
        unsafe { xglwall_render() }
    }
    pub fn size(&self) -> UVec2 {
        self.size
    }
    pub fn is_visible(&self) -> bool {
        unsafe { xglwall_is_visible() != 0 }
    }
    pub fn opengl_has_extension(name: &str) -> bool {
        let name = std::ffi::CString::new(name).unwrap();
        unsafe { xglwall_opengl_has_extension(name.as_ptr() as *const std::os::raw::c_char) != 0 }
    }
    pub unsafe fn get_proc_address(name: &str) -> *const std::ffi::c_void {
        let name = std::ffi::CString::new(name).unwrap();
        xglwall_opengl_proc(name.as_ptr() as *const std::os::raw::c_char)
    }
}
impl Drop for Xglwall {
    fn drop(&mut self) {
        unsafe {
            xglwall_cleanup();
        }
    }
}

pub struct XglwallGlium {
    size: UVec2,
}
impl XglwallGlium {
    pub fn new(xgl: &Xglwall) -> Self {
        Self { size: xgl.size() }
    }
}
unsafe impl glium::backend::Backend for XglwallGlium {
    fn swap_buffers(&self) -> Result<(), glium::SwapBuffersError> {
        Ok(())
    }
    unsafe fn get_proc_address(&self, symbol: &str) -> *const std::ffi::c_void {
        Xglwall::get_proc_address(symbol)
    }
    fn get_framebuffer_dimensions(&self) -> (u32, u32) {
        (self.size.x, self.size.y)
    }
    fn is_current(&self) -> bool {
        true
    }
    unsafe fn make_current(&self) {}
}

unsafe fn fill_key_event(ev: xglwall_event) -> KeyEvent {
    KeyEvent {
        keysym: ev.key.x11.keysym,
        keysym_mod: ev.key.x11.keysym_symbol,
        char: if ev.key.ascii != 0 {
            Some(ev.key.ascii as u8 as char)
        }
        else {
            None
        },
    }
}
unsafe fn fill_mouse_event(ev: xglwall_event) -> MouseEvent {
    MouseEvent {
        button: match ev.mouse.button as u32 {
            0 => MouseButton::None,
            xglwall_mouse_button_xglwall_mouse_button_left => MouseButton::Left,
            xglwall_mouse_button_xglwall_mouse_button_middle => MouseButton::Middle,
            xglwall_mouse_button_xglwall_mouse_button_right => MouseButton::Right,
            xglwall_mouse_button_xglwall_mouse_wheel_up => MouseButton::WheelUp,
            xglwall_mouse_button_xglwall_mouse_wheel_down => MouseButton::WheelDown,
            i => MouseButton::Unknown(i as i32),
        },
        pos: IVec2::new(ev.mouse.x, ev.mouse.y),
    }
}
