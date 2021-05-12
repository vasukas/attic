use std::time::Duration;
use glium::{self, uniform, Surface};

mod drawer;
mod drawer_life;
mod xglwall;

struct MainSleeper {
    target: Duration, // expected time between loop steps
    passed: Duration, // ...and actual one (or something close to it)

    time0: std::time::Instant, // time at the beginning of the loop
}
impl MainSleeper {
    fn default() -> Self {
        let t = Duration::from_secs_f64(1. / 60.);
        Self {
            target: t,
            passed: t,
            time0: std::time::Instant::now(),
        }
    }
    fn begin(&mut self) {
        self.time0 = std::time::Instant::now();
    }
    fn end(&mut self) {
        let time1 = std::time::Instant::now();
        self.passed = time1 - self.time0;
        if self.passed < self.target {
            std::thread::sleep(self.target - self.passed);
            self.passed = self.target;
        }
    }
}

fn main() {
    // xglwall

    let mut xgl = xglwall::Xglwall::init(xglwall::XglwallInit {
        title: "Window".to_string(),
        enable_input: true,
    })
    .unwrap();

    // init OpenGL

    let backend = xglwall::XglwallGlium::new(&xgl);
    let context =
        unsafe { glium::backend::Context::new(backend, false, Default::default()) }.unwrap();

    // init drawer

    let mut life = drawer_life::Life::new(5);
    life.init_rnd();
    life.load("init.tga");

    // init shader

    let program = glium::Program::from_source(
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
    .unwrap();

    // run

    let mut sleeper = MainSleeper::default();

    'main: loop {
        sleeper.begin();

        while let Some(ev) = xgl.poll_event() {
            match ev {
                xglwall::Event::KeyPress(xglwall::KeyEvent {
                    char: Some('q'), ..
                }) => {
                    break 'main;
                }
                xglwall::Event::Destroy => break 'main,
                _ => (),
            }
        }

        if !xgl.is_visible() {
            sleeper.end();
            continue;
        }

        life.update();

        // init vertices

        #[derive(Copy, Clone)]
        struct Vertex {
            p: [f32; 2], // this is the same name as parameter in shader
        }
        glium::implement_vertex!(Vertex, p);

        let mut vertices = Vec::new();
        vertices.reserve((life.size.x * life.size.y * 6) as usize);
        let w = life.size.x;
        for (i, alive) in life.state.iter().enumerate() {
            if *alive {
                let x = (i as u32 % w) as f32;
                let y = (i as u32 / w) as f32;
                let k = 0.5;
                vertices.push(Vertex { p: [x - k, y - k] });
                vertices.push(Vertex { p: [x + k, y - k] });
                vertices.push(Vertex { p: [x - k, y + k] });
                vertices.push(Vertex { p: [x + k, y - k] });
                vertices.push(Vertex { p: [x - k, y + k] });
                vertices.push(Vertex { p: [x + k, y + k] });
            }
        }

        let vertex_buffer = glium::VertexBuffer::new(&context, &vertices[..]).unwrap();

        // draw

        let mut target = glium::Frame::new(context.clone(), context.get_framebuffer_dimensions());
        target.clear_color(1., 1., 1., 1.);
        target
            .draw(
                &vertex_buffer,
                &glium::index::NoIndices(glium::index::PrimitiveType::TrianglesList),
                &program,
                &uniform! {
                k: [2. / (life.size.x as f32), 2. / (life.size.y as f32)],
                },
                &Default::default(),
            )
            .unwrap();
        target.finish().unwrap();

        xgl.render();
        sleeper.end();
    }
}
