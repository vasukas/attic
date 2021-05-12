use crate::drawer::*;
use rand::prelude::*;

pub struct Life {
    size_k: u32,
    size: UVec2,
    state: Vec<bool>,
    new_state: Vec<bool>,

    program: glium::Program,
}
impl Life {
    pub fn new(context: &mut Rc<glium::context::Context>, size_k: u32) -> Self {
        Self {
            size_k,
            size: UVec2::default(),
            state: Vec::new(),
            new_state: Vec::new(),

            program: glium::Program::from_source(
                    &context,
                    "#version 330
            layout(location = 0) in vec2 p;
            uniform vec2 k;
            //out vec2 tc;
            void main() {
            /*
                const vec2 tc_[6] = vec2[](
                    vec2(-1, -1),
                    vec2(1, -1),
                    vec2(-1, 1),
                    vec2(1, -1),
                    vec2(-1, 1),
                    vec2(1, 1)
                );
                tc = tc_[gl_VertexID % 6];
            */
                vec2 ou = p;
                ou = ou * k - 1;
                ou.y = -ou.y;
                gl_Position = vec4(ou, 0, 1);
            }
            ",
                    "#version 330
            //in vec2 tc;
            out vec4 ret;
            void main() {
                ret = vec4(vec3(0), 1);
            }
            ",
                    None,
                )
                .unwrap()
        }
    }
    pub fn load(&mut self, filename: &str) {
        let data = std::fs::read(filename).unwrap();
        if data.len() < 12 {
            panic!("TGA: invalid file size (header)");
        }
        let frakkin_id = data[0];
        if data[1] != 0 {
            panic!("TGA: contains colormap");
        }
        if data[2] != 2 {
            panic!("TGA: not uncompressed RGB");
        }
        let width = data[12] as u32 | ((data[13] as u32) << 8);
        let height = data[14] as u32 | ((data[15] as u32) << 8);
        if data[16] != 24 {
            panic!("TGA: not 24-bit RGB");
        }
        /*
        if data[17] != 0 {
            panic!("TGA: invalid image descriptor");
        }
        */
        let data = &data[18 + (frakkin_id as usize)..];
        if data.len() < (width * height * 3) as usize {
            panic!("TGA: invalid file size (data)");
        }
        if width > self.size.x || height > self.size.y {
            panic!("TGA: image bigger than field");
        }
        let x_off = (self.size.x - width) / 2;
        let y_off = (self.size.y - height) / 2;
        for i in 0..(width * height) as usize {
            self.state
                [((y_off + i as u32 / width) * self.size.x + (x_off + i as u32 % width)) as usize] = {
                if data[i * 3] == 0 && data[i * 3 + 1] == 0 && data[i * 3 + 2] == 0 {
                    true
                }
                else {
                    false
                }
            }
        }
    } 
    pub fn init_rnd(&mut self) {
        let mut rng = rand::thread_rng();
        for alive in self.state.iter_mut() {
            *alive = rng.gen::<f32>() < 0.2;
        }
    }
}
impl Drawer for Life {
    fn set_size(&mut self, size: UVec2) {
        let size = size / self.size_k;

        self.state.resize((size.x * size.y) as usize, false);
        self.new_state.resize((size.x * size.y) as usize, false);
        self.size = size;
    }
    fn update(&mut self, _: Duration) {
        let w = self.size.x as i32;
        for (i, alive) in self.state.iter().enumerate() {
            let mut neis = 0;
            // TODO: fix offsets, wrapping is invalid
            for offset in [-1, 1, -w - 1, -w, -w + 1, w - 1, w, w + 1].iter() {
                if self.state[((i as i32 + *offset) + self.state.len() as i32) as usize % self.state.len()] {
                    neis += 1;
                }
            }
            self.new_state[i] = if *alive {
                if neis == 2 || neis == 3 {
                    true
                }
                else {
                    false
                }
            }
            else {
                if neis == 3 {
                    true
                }
                else {
                    false
                }
            };
        }
        std::mem::swap(&mut self.state, &mut self.new_state);
    }
    fn render(&mut self, target: &mut glium::Frame) {
    }
}