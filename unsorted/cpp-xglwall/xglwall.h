/*
	Simple X11/OpenGL wallpaper engine
	
	Compile with following pkg-config packages: x11, xrender, glx. 
*/

#ifndef XGLWALL_H
#define XGLWALL_H

#ifdef __cplusplus
extern "C" {
#endif

typedef int boolean;

typedef enum xglwall_log_type {
	xglwall_log_debug, // various messages
	xglwall_log_error  // description of error
} xglwall_log_type;

// called for entire messages, doesn't contain newline
typedef void(*xglwall_log_func)(xglwall_log_type type, const char *s, int len, void* userdata);

// events
typedef enum xglwall_event_type {
	xglwall_event_destroy, // request to close window
	// .key
	xglwall_event_key_press,
	xglwall_event_key_release,
	// .mouse
	xglwall_event_mouse_press,
	xglwall_event_mouse_release,
	xglwall_event_mouse_move,
	// .resize
	xglwall_event_window_resize,
} xglwall_event_type;

typedef enum xglwall_mouse_button {
	xglwall_mouse_button_left = 1,
	xglwall_mouse_button_middle,
	xglwall_mouse_button_right,
	xglwall_mouse_wheel_up,
	xglwall_mouse_wheel_down,
} xglwall_mouse_button;

typedef union xglwall_event {
	struct {
		struct {
			int keycode; // platform-dependent
			int keysym; // KeySym, see X11/keysymdef.h for values
			int keysym_symbol; // same as keysym, but with modifiers applied
		} x11;
		char symbol[16]; // null-terminated string, with modifiers applied
		char ascii; // set to ASCII value or 0 if key doesn't have one
	} key;
	struct {
		int button; // xglwall_mouse_button and other values; 0 for move event
		int x, y; // relative
	} mouse;
	struct {
		int width;
		int height;
	} resize;
} xglwall_event;

typedef void(*xglwall_event_handler)(xglwall_event_type type, xglwall_event event);

// init
typedef struct xglwall_params_opengl {
	boolean doublebuffer;
	int red_bits;
	int green_bits;
	int blue_bits;
	int alpha_bits;
	int depth_bits;
	int stencil_bits;
	int gl_major; // minimal OpenGL version
	int gl_minor;
} xglwall_params_opengl;

typedef struct xglwall_init_params {
	const char *title; // if NULL, "xglwall" is used
	xglwall_log_func log; // if NULL, outputs to stdout/stderr with "xglwall: " prefix
	void* log_userdata;
	const char *display_name; // if NULL, default is used
	boolean enable_input; // even if false, you still should call xglwall_process_events
	
	xglwall_params_opengl opengl;
} xglwall_init_params;

typedef struct xglwall_init_output {
	int width; // pixels
	int height;
	
	xglwall_params_opengl opengl; // what we actually got
} xglwall_init_output;

// Create window & GL context
// On error cleanup is called before returning
// Values in 'init_out' are undefined on failure
boolean xglwall_init(const xglwall_init_params* init, xglwall_init_output* init_out);

// Safe to call even if 'init' never was called
void xglwall_cleanup();

// Get a single event. Returns false if no waiting events left
boolean xglwall_poll_event(xglwall_event_type* ret_type, xglwall_event* ret_data);

// Process all pending events via a handler. Returns false if got 'destroyed' event (not sent to handler). 
// Handler can be null - when only internal processing is done
boolean xglwall_process_events(xglwall_event_handler handler);

// Render framebuffer to screen (swap window)
void xglwall_render();

// Checks if at least some part of the window is visible to user. 
// This function almost never works
boolean xglwall_is_visible();

// Utility functions

// Doublebuffer, 32-bit RGBA, 24 depth, no stencil, version 3.3
xglwall_params_opengl xglwall_params_opengl_default();

// Checks if extension is available in current context
boolean xglwall_opengl_has_extension(const char *extension);

// Returns OpenGL procedure from current context
void* xglwall_opengl_proc(const char *name);

// Expects xglwall_event::key::x11::keysym
const char *xglwall_keysym_name(int keysym);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // XGLWALL_H
