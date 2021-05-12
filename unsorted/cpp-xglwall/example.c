#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <GL/glew.h>
#include "xglwall.h"

#define LOG(FMT, ...)   fprintf(stdout, FMT "\n", ##__VA_ARGS__)
#define ERROR(FMT, ...) fprintf(stderr, FMT "\n", ##__VA_ARGS__)

static GLuint make_shader_program(const char *vert, const char *frag) {
	GLuint sh[2];
	for (int i=0; i<2; ++i) {
		GLenum type = i==0 ? GL_VERTEX_SHADER : GL_FRAGMENT_SHADER;
		const char *src = i==0 ? vert : frag;
		
		sh[i] = glCreateShader(type);
		glShaderSource(sh[i], 1, &src, NULL);
		glCompileShader(sh[i]);
		
		int status = 0;
		glGetShaderiv(sh[i], GL_COMPILE_STATUS, &status);
		if (status == GL_FALSE) {
			ERROR("Shader compilation failed (%s)", i==0? "vert" : "frag");
			
			GLint str_n = 0;
			glGetShaderiv(sh[i], GL_INFO_LOG_LENGTH, &str_n);
			char *str = malloc(str_n);
			GLsizei len = 0;
			glGetShaderInfoLog(sh[i], str_n, &len, str);
			
			ERROR("%s", str);
			free(str);
			
			return 0;
		}
	}
	
	GLuint sp = glCreateProgram();
	for (int i=0; i<2; ++i) {
		glAttachShader(sp, sh[i]);
	}
	glLinkProgram(sp);
	
	int status = 0;
	glGetProgramiv(sp, GL_LINK_STATUS, &status);
	if (status == GL_FALSE) {
		ERROR("Shader link failed");
		
		GLint str_n = 0;
		glGetProgramiv(sp, GL_INFO_LOG_LENGTH, &str_n);
		char *str = malloc(str_n);
		GLsizei len = 0;
		glGetProgramInfoLog(sp, str_n, &len, str);
		
		ERROR("%s", str);
		free(str);
		
		return 0;
	}
	
	for (int i=0; i<2; ++i) {
		glDetachShader(sp, sh[i]);
		glDeleteShader(sh[i]);
	}
	
	glUseProgram(sp);
	return sp;
}

static const char *gldbg_type_name(GLenum x) {
	switch (x) {
		case GL_DEBUG_TYPE_ERROR: return "Error";
		case GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR: return "Deprecated";
		case GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR: return "UB";
		case GL_DEBUG_TYPE_PORTABILITY: return "Portability";
		case GL_DEBUG_TYPE_PERFORMANCE: return "Performance";
		default: return "Unknown";
	}
}
static const char *gldbg_sever_name(GLenum x) {
	switch (x) {
		case GL_DEBUG_SEVERITY_HIGH:   return "High";
		case GL_DEBUG_SEVERITY_MEDIUM: return "Medium";
		case GL_DEBUG_SEVERITY_LOW:    return "Low";
		case GL_DEBUG_SEVERITY_NOTIFICATION: return "Info";
		default: return "Unknown";
	}
}
static void GLAPIENTRY gldbg_func(GLenum _1, GLenum type, GLuint _2, GLenum sever, GLsizei length, const GLchar* msg, const void* _3) {
	(void) _1; (void) _2; (void) _3; (void) length;
	LOG("GL [%s/%s]: %s", gldbg_type_name(type), gldbg_sever_name(sever), msg);
}

static boolean run = 1;

static float xk;
static float yk;

static GLint sp_pos;
static GLint sp_clr;

static void set_pos(int x, int y) {
	glUniform2f(sp_pos, x * xk, y * yk);
}
static void set_color(float topleft, float botright, float pos) {
	glUniform3f(sp_clr, topleft, botright, pos);
}

static int key_state = 0;

static void on_button(int x, int y) {
	set_pos(x, y);
}

static void on_key() {
	if (key_state == 0) {
		set_color(0, 1, 0);
	}
	else {
		set_color(1, 0, 1);
	}
	key_state = !key_state;
}

static void on_event(xglwall_event_type type, xglwall_event ev) {
	switch (type) {
		case xglwall_event_key_press:
			{
				char s[2] = {ev.key.ascii ? ev.key.ascii : '.', 0};
				printf("Got a key: %s\n", s);
			}
			if (ev.key.ascii == 'q') {
				run = 0;
			}
			on_key();
			break;
			
		case xglwall_event_mouse_press:
			printf("Got a button: %d\n", ev.mouse.button);
			on_button(ev.mouse.x, ev.mouse.y);
			break;
			
		case xglwall_event_mouse_move:
			on_button(ev.mouse.x, ev.mouse.y);
			break;
	default:;
	}
}

int main()
{
	// xglwall
	
	xglwall_init_params init = {};
	init.opengl = xglwall_params_opengl_default();
	init.enable_input = 1;
	
	xglwall_init_output init_out;
	
	if (!xglwall_init(&init, &init_out)) {
		ERROR("xglwall_init failed");
		return 1;
	}
	
	xk = 1.f / init_out.width;
	yk = 1.f / init_out.height;
	
	// init OpenGL
	
	glewExperimental = 1;
	GLint err = glewInit();
	if (err != GLEW_OK) {
		ERROR("glewInit failed - %d", err);
		return 1;
	}
	
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE);
	
	glDisable(GL_CULL_FACE);
	glDisable(GL_DEPTH_TEST);
	glDepthMask(0);
	
	// log OpenGL errors
	
	if (glDebugMessageCallback) {
		while (glGetError()) ;
		glEnable(GL_DEBUG_OUTPUT);
		glDebugMessageCallback(gldbg_func, NULL);
		
		if (glGetError() == GL_NO_ERROR) {
			LOG("glDebugMessageCallback set");
		}
		else {
			glDisable(GL_DEBUG_OUTPUT);
			ERROR("glDebugMessageCallback set failed");
		}
	}
	else {
		ERROR("glDebugMessageCallback not available");
	}
	
	// init shader
	
	GLuint sp = make_shader_program(
		"#version 330\n"
		"layout(location = 0) in vec2 p;"
		"out vec2 tc;"
		"void main() {"
		"    tc = p * 0.5 + 0.5;"
		"    tc.y = 1 - tc.y;"
		"    gl_Position = vec4(p, 0, 1);"
		"}\n",

		"#version 330\n"
		"uniform vec2 pos;"
		"uniform vec3 clr;"
		"in vec2 tc;"
		"out vec4 ret;"
		"void main() {"
		"    float dist = length(tc - pos);"
		"    ret = vec4("
		"        mix(clr.r, 1 - clr.r, tc.x),"
		"        mix(clr.g, 1 - clr.g, tc.y),"
		"        mix(clr.b, 1 - clr.b, dist / 0.05),"
		"        mix(clr.r, 1 - clr.r, tc.x)"
		"    );"
		"}\n"
	);
	sp_pos = glGetUniformLocation(sp, "pos");
	sp_clr = glGetUniformLocation(sp, "clr");
	if (!sp || sp_pos == -1 || sp_clr == -1) {
		ERROR("Shader init failed");
		return 1;
	}
	
	set_pos(200, 200);
	set_color(1, 0, 1);
	
	// init vertices (full-screen quad in NDC)
	
	GLuint vao;
	glGenVertexArrays(1, &vao);
	glBindVertexArray(vao);
	
	const GLfloat ps[] = {
	    -1, -1,   1, -1,   -1, 1,
	              1, -1,   -1, 1,   1, 1
	};
	GLuint vbo;
	glGenBuffers(1, &vbo);
	glBindBuffer(GL_ARRAY_BUFFER, vbo);
	glBufferData(GL_ARRAY_BUFFER, 12 * sizeof(GLfloat), ps, GL_STATIC_DRAW);
	
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, 0);
	
	// run
	
	const int sleep_time = 1000 * 1000 / 60; // microseconds, 60 fps (16.(6) ms)
	int was_visible = 0;
	
	while (run) {
		if (!xglwall_process_events(on_event)) {
			break;
		}
		
		if (!xglwall_is_visible()) {
			if (was_visible) {
				LOG("Now OBSCURED!");
				was_visible = 0;
			}
			
			usleep(sleep_time);
			continue;
		}
		if (!was_visible) {
			LOG("Now VISIBLE!");
			was_visible = 1;
		}
		
		glClearColor(0, 0, 0, 0);
		glClear(GL_COLOR_BUFFER_BIT);
		glDrawArrays(GL_TRIANGLES, 0, 6);
		
		xglwall_render();
	}
	
	// cleanup
	
	LOG("Cleanup");
	
	xglwall_cleanup();
	
	return 0;
}
