/*------------------------------------------------------------------------
 * A demonstration of OpenGL in a  ARGB window 
 *    => support for composited window transparency
 *
 * (c) 2011 by Wolfgang 'datenwolf' Draxinger
 *     See me at comp.graphics.api.opengl and StackOverflow.com
  
 * License agreement: This source code is provided "as is". You
 * can use this source code however you want for your own personal
 * use. If you give this source code to anybody else then you must
 * leave this message in it.
 *
 * This program is based on the simplest possible 
 * Linux OpenGL program by FTB (see info below)
 
  The simplest possible Linux OpenGL program? Maybe...
  (c) 2002 by FTB. See me in comp.graphics.api.opengl
  --
  <\___/>
  / O O \
  \_____/  FTB.
------------------------------------------------------------------------*/
// Everything's re-structured and extended, though

#include "xglwall.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include <sys/types.h>
#include <time.h>

#include <GL/gl.h>
#include <GL/glx.h>
#include <GL/glxext.h>
#include <X11/Xatom.h>
#include <X11/extensions/Xrender.h>
#include <X11/Xutil.h>

// TODO: test whatever is possible, refactor, etc

// VisibilityNotify doesn't work with compositors,
//   and either _NET_WM_BYPASS_COMPOSITOR doesn't work in Openbox,
//   or visibility for desktop-sized window always true,
//   or something else, but I couldn't get it to work.
// Maybe X11 doesn't calculate full visibility, dunno.
#define USE_VISIBILITY_EVENT 0

static xglwall_log_func log_func = NULL;
static void* log_userdata = NULL;

static Display *dpy = NULL;
static int screen = 0;
static Window root = 0;
static Atom del_atom = 0; // persistent across init/cleanup calls - Atoms don't have deallocation

static Window wnd = 0;
static GLXContext glctx = NULL;
static boolean fully_obscured = False;

// === Logging

#define LOG(FMT, ...)   log_func_wrapper(xglwall_log_debug, FMT, ##__VA_ARGS__)
#define ERROR(FMT, ...) log_func_wrapper(xglwall_log_error, FMT, ##__VA_ARGS__)

static void log_func_wrapper(xglwall_log_type type, const char *fmt, ...) __attribute__ ((format (printf, 2, 3)));
void log_func_wrapper(xglwall_log_type type, const char *fmt, ...) {
	const int bn = 4096;
	char b[bn];
	va_list args;
	va_start(args, fmt);
	int n = vsnprintf(b, bn, fmt, args);
	va_end(args);
	if (n > 0) {
		log_func(type, b, n, log_userdata);
	}
}
static void default_log_func(xglwall_log_type type, const char *s, int n, void* u) {
	(void) u;
	FILE* file = type == xglwall_log_debug ? stdout : stderr;
	fwrite("xglwall: ", 1, 9, file);
	fwrite(s, 1, n, file);
	fwrite("\n", 1, 1, file);
}

// === Init

static int glx_error_handler(Display* dpy, XErrorEvent* ev) {
	(void) dpy; (void) ev;
    ERROR("Some GLX context creation error has occured");
    return 0;
}

boolean xglwall_init(const xglwall_init_params* init, xglwall_init_output* init_out) {
	xglwall_cleanup();
	
	// init variables	
	
	log_func = init->log;
	log_userdata = init->log_userdata;
	if (!log_func) {
		log_func = default_log_func;
		log_userdata = NULL;
	}
	
	init_out->opengl = init->opengl;
	
	fully_obscured = False;
	
	// get display
	
	dpy = XOpenDisplay(init->display_name);
	if (!dpy) {
		ERROR("XOpenDisplay failed");
		return False;
	}
	screen = DefaultScreen(dpy);
	root = RootWindow(dpy, screen);
	
	// build FB params
	
	const int cfg_pars[] = {
	    GLX_RENDER_TYPE, GLX_RGBA_BIT,
	    GLX_DRAWABLE_TYPE, GLX_WINDOW_BIT,
	    GLX_DOUBLEBUFFER, init->opengl.doublebuffer,
	    GLX_RED_SIZE, init->opengl.red_bits,
		GLX_GREEN_SIZE, init->opengl.green_bits,
		GLX_BLUE_SIZE, init->opengl.blue_bits,
		GLX_ALPHA_SIZE, init->opengl.alpha_bits,
		GLX_DEPTH_SIZE, init->opengl.depth_bits,
	    GLX_STENCIL_SIZE, init->opengl.stencil_bits,
		None
	};
	
	// get FB config
	
	GLXFBConfig fbcfg = NULL;
	XVisualInfo *visual = NULL;
	XRenderPictFormat *pict_format = NULL;
	
	int n_fbcfgs = 0;
	GLXFBConfig *s_fbcfg = glXChooseFBConfig(dpy, screen, cfg_pars, &n_fbcfgs);
	if (!n_fbcfgs) {
		ERROR("glXChooseFBConfig failed (no configs)");
		goto failed;
	}
	
	for (int i=0; i<n_fbcfgs; ++i) {
		visual = glXGetVisualFromFBConfig(dpy, s_fbcfg[i]);
		if (!visual) {
			continue;
		}

		pict_format = XRenderFindVisualFormat(dpy, visual->visual);
		if (!pict_format) {
			continue;
		}

		fbcfg = s_fbcfg[i];
		if (!init->opengl.alpha_bits || pict_format->direct.alphaMask > 0) {
			break;
		}
		fbcfg = NULL; // thx clang
	}
	
	if (!fbcfg) {
		ERROR("No matching GLXFBConfig");
		goto failed;
	}
	
	// output FB config
	
	glXGetFBConfigAttrib(dpy, fbcfg, GLX_DOUBLEBUFFER, &init_out->opengl.doublebuffer);
	glXGetFBConfigAttrib(dpy, fbcfg, GLX_RED_SIZE, &init_out->opengl.red_bits);
	glXGetFBConfigAttrib(dpy, fbcfg, GLX_GREEN_SIZE, &init_out->opengl.green_bits);
	glXGetFBConfigAttrib(dpy, fbcfg, GLX_BLUE_SIZE, &init_out->opengl.blue_bits);
	glXGetFBConfigAttrib(dpy, fbcfg, GLX_ALPHA_SIZE, &init_out->opengl.alpha_bits);
	glXGetFBConfigAttrib(dpy, fbcfg, GLX_DEPTH_SIZE, &init_out->opengl.depth_bits);
	glXGetFBConfigAttrib(dpy, fbcfg, GLX_STENCIL_SIZE, &init_out->opengl.stencil_bits);
	
	LOG("Selected GLXFBConfig:");
	LOG("  doublebuffer = %d", init_out->opengl.doublebuffer);
	LOG("  rgba = %d/%d/%d/%d bits",
	    init_out->opengl.red_bits,
	    init_out->opengl.green_bits,
	    init_out->opengl.blue_bits,
	    init_out->opengl.alpha_bits
	);
	LOG("  depth/stencil = %d/%d bits",
	    init_out->opengl.depth_bits,
	    init_out->opengl.stencil_bits
	);
	
	// fill window attributes
	
	Colormap cmap = XCreateColormap(dpy, root, visual->visual, AllocNone);
	
	XSetWindowAttributes attr = {};
	attr.colormap = cmap;
	attr.background_pixmap = None;
	attr.border_pixmap = None;
	attr.border_pixel = 0;
	
#if USE_VISIBILITY_EVENT
	attr.event_mask |= VisibilityChangeMask;
#endif
	
	if (init->enable_input) {
		attr.event_mask |=
			StructureNotifyMask | /* for ConfigureNotify */
			KeyPressMask |
			KeyReleaseMask |
			ButtonPressMask |
			ButtonReleaseMask |
			PointerMotionMask;
	}
	
	int attr_mask = 
		CWColormap |
		CWBorderPixel |
		CWEventMask;
	
	// make window dimensions
	
	int x = 0;
	int y = 0;
	int width = DisplayWidth(dpy, screen);
	int height = DisplayHeight(dpy, screen);
	
	init_out->width = width;
	init_out->height = height;
	
	LOG("Window dimensions: %dx%d+%d+%d", width, height, x, y);
	
	// create window
	
	wnd = XCreateWindow(
		dpy,
		root,
		x, y, width, height,
		0,
		visual->depth,
		InputOutput,
		visual->visual,
		attr_mask, &attr
	);
	if (!wnd) {
		ERROR("XCreateWindow failed");
		goto failed;
	}
	
	// set window manager properties
	
	const char *title_str = init->title;
	if (!title_str) {
		title_str = "xglwall";
	}
	
	XTextProperty title = {};
	title.value = (unsigned char *) title_str;
	title.encoding = XA_STRING;
	title.format = 8;
	title.nitems = strlen(title_str);
	
	XSizeHints hints = {};
	hints.x = x;
	hints.y = y;
	hints.width = width;
	hints.height = height;
	hints.flags = USPosition | USSize;
	
	XWMHints *wmhints = XAllocWMHints();
	wmhints->initial_state = NormalState;
	wmhints->flags = StateHint;

	XSetWMProperties(dpy, wnd, &title, &title,
		NULL, 0,
		&hints,
		wmhints,
		NULL
	);

	XFree(wmhints);
	
	// set window type - 'desktop' is placed below all other windows
	
	Atom prop = XInternAtom(dpy, "_NET_WM_WINDOW_TYPE", False);
	Atom modes[] = {
	    XInternAtom(dpy, "_NET_WM_WINDOW_TYPE_DESKTOP", False)
	};
	XChangeProperty(dpy, wnd, prop, XA_ATOM, 32, PropModeReplace, (uint8_t*) modes, 1);
	
	// set borderless
	
	struct MwmHints {
	    unsigned long flags;
	    unsigned long functions;
	    unsigned long decorations;
	    long input_mode;
	    unsigned long status;
	};
	enum {
	    MWM_HINTS_DECORATIONS = (1L << 1)
	};
	
	Atom mwmHintsProperty = XInternAtom(dpy, "_MOTIF_WM_HINTS", 0);
	struct MwmHints motif_hints = {};
	motif_hints.flags = MWM_HINTS_DECORATIONS;
	motif_hints.decorations = 0;
	XChangeProperty(dpy, wnd, mwmHintsProperty, mwmHintsProperty, 32,
		PropModeReplace, (unsigned char *) &motif_hints, 5
	);
	
	// hide from taskbar
	
	prop = XInternAtom(dpy, "_NET_WM_STATE", False);
	Atom data = XInternAtom(dpy, "_NET_WM_STATE_SKIP_TASKBAR", False);
	XChangeProperty(dpy, wnd, prop, XA_ATOM, 32, PropModeReplace, (uint8_t*) &data, 1);
	
	// set compositor preference - disable
	
#if USE_VISIBILITY_EVENT
	prop = XInternAtom(dpy, "_NET_WM_BYPASS_COMPOSITOR", False);
	Atom values[] = { 1 };
	XChangeProperty(dpy, wnd, prop, XA_ATOM, 32, PropModeReplace, (uint8_t*) &values, 1);
#endif
	
	// make window visible (MUST be after window hints)
	
	XMapWindow(dpy, wnd);
	
	// get atom for WM's request to close window
	
	if (del_atom == None) { // may be left from previous init
		del_atom = XInternAtom(dpy, "WM_DELETE_WINDOW", False);
	}
	if (del_atom != None) {
		XSetWMProtocols(dpy, wnd, &del_atom, 1);
	}
	
	// do we really have OpenGL?
	
	int garbage = 0;
	if (!glXQueryExtension(dpy, &garbage, &garbage)) {
		ERROR("glXQueryExtension failed (OpenGL not supported by X server)");
		goto failed;
	}
	
	// init GL context
	
	if (!xglwall_opengl_has_extension("GLX_ARB_create_context")) {
		LOG("GLX_ARB_create_context: not supported");
	}
	else {
		typedef GLXContext(*glXCreateContextAttribsARBProc)(Display*, GLXFBConfig, GLXContext, Bool, const int*);
		glXCreateContextAttribsARBProc glXCreateContextAttribsARB =
			(glXCreateContextAttribsARBProc) xglwall_opengl_proc("glXCreateContextAttribsARB");
		
		if (!glXCreateContextAttribsARB) {
			ERROR("Failed to get GLX_ARB_create_context (null), even though extension is available");
		}
		else {
			int context_attribs[] = {
				GLX_CONTEXT_MAJOR_VERSION_ARB, init->opengl.gl_major,
				GLX_CONTEXT_MINOR_VERSION_ARB, init->opengl.gl_minor,
//				GLX_CONTEXT_FLAGS_ARB, GLX_CONTEXT_FORWARD_COMPATIBLE_BIT_ARB,
				None
			};

			int (*oldHandler)(Display*, XErrorEvent*) = XSetErrorHandler(glx_error_handler);
			glctx = glXCreateContextAttribsARB(dpy, fbcfg, 0, True, context_attribs);
			XSync(dpy, False);
			XSetErrorHandler(oldHandler);
			
			if (glctx) {
				LOG("GLX_ARB_create_context: ok");
			}
			else {
				LOG("GLX_ARB_create_context: failed");
			}
		}
	}
	
	if (!glctx) {
		ERROR("OpenGL version can't be set, using default");
		
		glctx = glXCreateNewContext(dpy, fbcfg, GLX_RGBA_TYPE, 0, True);
		if (!glctx) {
			ERROR("glXCreateNewContext failed");
			goto failed;
		}
	}
	
	if (!glXMakeContextCurrent(dpy, wnd, wnd, glctx)) {
		ERROR("glXMakeContextCurrent failed");
		goto failed;
	}
	
	// whoo
	
	XFree(visual);
	XFree(s_fbcfg);
	
	LOG("xglwall_init() successful");
	return True;
	
failed:
	XFree(visual);
	XFree(s_fbcfg);
	
	xglwall_cleanup();
	return False;
}

// === Cleanup

void xglwall_cleanup() {
	// TODO: is that all?
	
	if (glctx) {
		glXDestroyContext(dpy, glctx);
		glctx = NULL;
	}
	
	if (wnd) {
		XDestroyWindow(dpy, wnd);
		wnd = 0;
	}
	
	dpy = NULL;
	screen = 0;
	root = 0;
}

// === Events

static boolean event_internal(XEvent* event, xglwall_event_type* ret_type, xglwall_event* ret_data) {
	(void) ret_data;
	
	switch (event->type) {
		case ClientMessage: // received regardless of event mask
			if ((Atom) event->xclient.data.l[0] == del_atom) {
				if (ret_type) {
					*ret_type = xglwall_event_destroy;
				}
				return False;
			}
			break;
			
		case MappingNotify: // received regardless of event mask
			XRefreshKeyboardMapping(&event->xmapping);
			break;
			
		// this doesn't work with a compositor, and almost everyone uses them
		case VisibilityNotify:
			switch (event->xvisibility.state) {
				case VisibilityUnobscured:
				case VisibilityPartiallyObscured:
					fully_obscured = False;
					break;
					
				case VisibilityFullyObscured:
					fully_obscured = True;
					break;
			}
			break;
	}
	return True;
}

static void fill_key_event(xglwall_event* ev, XKeyEvent* xkey) {
	ev->key.x11.keycode = xkey->keycode;
	ev->key.x11.keysym = XLookupKeysym(xkey, 0);
	
	KeySym keysym_symbol;
	int n = XLookupString(xkey, ev->key.symbol, sizeof(ev->key.symbol) - 1, &keysym_symbol, NULL);
	ev->key.symbol[n] = 0;
	ev->key.x11.keysym_symbol = keysym_symbol;
	
	ev->key.ascii = (n == 1) ? ev->key.symbol[0] : 0;
}

boolean xglwall_poll_event(xglwall_event_type* ret_type, xglwall_event* ret_data) {
	while (XPending(dpy)) {
		XEvent event;
		XNextEvent(dpy, &event);
		event_internal(&event, ret_type, ret_data);
		
		XConfigureEvent *xc;
		
		switch (event.type) {
			case ConfigureNotify:
				xc = &(event.xconfigure);
				ret_data->resize.width = xc->width;
				ret_data->resize.height = xc->height;
				*ret_type = xglwall_event_window_resize;
				return True;
				
			case KeyPress:
				fill_key_event(ret_data, &event.xkey);
				*ret_type = xglwall_event_key_press;
				return True;
			
			case KeyRelease:
				fill_key_event(ret_data, &event.xkey);
				*ret_type = xglwall_event_key_release;
				return True;
				
			case ButtonPress:
				ret_data->mouse.button = event.xbutton.button;
				ret_data->mouse.x = event.xbutton.x;
				ret_data->mouse.y = event.xbutton.y;
				*ret_type = xglwall_event_mouse_press;
				return True;
			
			case ButtonRelease:
				ret_data->mouse.button = event.xbutton.button;
				ret_data->mouse.x = event.xbutton.x;
				ret_data->mouse.y = event.xbutton.y;
				*ret_type = xglwall_event_mouse_release;
				return True;
				
			case MotionNotify:
				ret_data->mouse.button = 0;
				ret_data->mouse.x = event.xmotion.x;
				ret_data->mouse.y = event.xmotion.y;
				*ret_type = xglwall_event_mouse_move;
				return True;
		}
	}
	return False;
}

boolean xglwall_process_events(xglwall_event_handler handler) {
	if (!handler) {
		while (XPending(dpy)) {
			XEvent event;
			XNextEvent(dpy, &event);
			if (!event_internal(&event, NULL, NULL)) {
				return False;
			}
		}
		return True;
	}
	
	xglwall_event_type type;
	xglwall_event event;
	while (xglwall_poll_event(&type, &event)) {
		if (type == xglwall_event_destroy) {
			return False;
		}
		else {
			handler(type, event);
		}
	}
	return True;
}

// === Render

void xglwall_render() {
	glXSwapBuffers(dpy, wnd);
}

boolean xglwall_is_visible() {
	return !fully_obscured;
}

// === Utility

xglwall_params_opengl xglwall_params_opengl_default() {
	xglwall_params_opengl pars = {};
	pars.doublebuffer = 1;
	pars.red_bits = 8;
	pars.green_bits = 8;
	pars.blue_bits = 8;
	pars.alpha_bits = 8;
	pars.depth_bits = 24;
	pars.stencil_bits = 0;
	pars.gl_major = 3;
	pars.gl_minor = 3;
	return pars;
}

// only function left from source without modification
boolean xglwall_opengl_has_extension(const char *extension) {
	const char *extList = glXQueryExtensionsString(dpy, screen);
	if (!extList) {
		return False;
	}
	
	/* Extension names should not have spaces. */
	const char *where = strchr(extension, ' ');
	if (where || *extension == '\0') {
		return False;
	}
	
	/* It takes a bit of care to be fool-proof about parsing the
       OpenGL extensions string. Don't be fooled by sub-strings,
       etc. */
	for (const char *start = extList ;; ) {
		where = strstr(start, extension);
		if (!where) {
			break;
		}
		
		const char *terminator = where + strlen(extension);
		if ((where == start || *(where - 1) == ' ') &&
		    (*terminator == ' ' || *terminator == '\0')) {
				return True;
		}
		
		start = terminator;
	}
	return False;
}

void* xglwall_opengl_proc(const char *name) {
	return glXGetProcAddressARB((const GLubyte *) name);
}

const char *xglwall_keysym_name(int keysym) {
	return XKeysymToString(keysym);
}
