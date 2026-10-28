/**************************************************************************/
/*  chimera_display.cpp                                                   */
/**************************************************************************/
/* See chimera_display.h.                                                 */
/**************************************************************************/

#include "chimera_display.h"

#include "chimera_os.h"

#include "core/input/input.h"
#include "drivers/gles3/rasterizer_gles3.h"

#include <glad/egl.h>
#include <glad/gl.h>

/* OSMesa's entry points, declared rather than included: <GL/osmesa.h>
 * brings Mesa's own <GL/gl.h>, which fights glad's. */
extern "C" {
typedef struct osmesa_context *OSMesaContext;
typedef void (*OSMESAproc)();
OSMesaContext OSMesaCreateContextAttribs(const int *attribList, OSMesaContext sharelist);
GLboolean OSMesaMakeCurrent(OSMesaContext ctx, void *buffer, GLenum type, GLsizei width, GLsizei height);
OSMESAproc OSMesaGetProcAddress(const char *funcName);
}
#define OSMESA_BGRA 0x1
#define OSMESA_FORMAT 0x22
#define OSMESA_DEPTH_BITS 0x30
#define OSMESA_STENCIL_BITS 0x31
#define OSMESA_ACCUM_BITS 0x32
#define OSMESA_PROFILE 0x33
#define OSMESA_CORE_PROFILE 0x34
#define OSMESA_CONTEXT_MAJOR_VERSION 0x36
#define OSMESA_CONTEXT_MINOR_VERSION 0x37

// Godot's renderer looks its OpenGL up through eglGetProcAddress when there
// is one (drivers/gles3/rasterizer_gles3.cpp); this is the one it finds.
static __eglMustCastToProperFunctionPointerType _osmesa_get_proc(const char *p_name) {
	return (__eglMustCastToProperFunctionPointerType)OSMesaGetProcAddress(p_name);
}

Vector<String> DisplayServerChimera::get_rendering_drivers_func() {
	Vector<String> drivers;
	drivers.push_back("opengl3");
	return drivers;
}

void DisplayServerChimera::register_chimera_driver() {
	register_create_function("chimera", create_func, get_rendering_drivers_func);
}

// The size the core allows a window to have (waterbox.config's capacity).
extern "C" void chimera_godot_max_picture(int *r_width, int *r_height);
// The share of the size the game asks for that the window is given, in
// percent (the "drawingSize" setting).
extern "C" int chimera_godot_drawing_percent();

DisplayServer *DisplayServerChimera::create_func(const String &p_rendering_driver, DisplayServerEnums::WindowMode p_mode, DisplayServerEnums::VSyncMode p_vsync_mode, uint32_t p_flags, const Vector2i *p_position, const Vector2i &p_resolution, int p_screen, DisplayServerEnums::Context p_context, int64_t p_parent_window, Error &r_error) {
	int mw = 1920, mh = 1080;
	chimera_godot_max_picture(&mw, &mh);
	DisplayServerChimera *ds = memnew(DisplayServerChimera(p_resolution, Size2i(mw, mh), r_error));
	if (r_error != OK) {
		memdelete(ds);
		return nullptr;
	}
	ds->window_mode = p_mode;
	ds->vsync_mode = p_vsync_mode;
	return ds;
}

// The window a size asked for gets: the drawing size's share of it, within
// what the core allows. The game is told this size and draws at it; a game
// that stretches its picture to its window (Godot's canvas_items or
// disabled stretch modes) draws fewer pixels, one with a fixed size of its
// own (viewport) as many as before.
static Size2i _clamp_size(const Size2i &p_size, const Size2i &p_max) {
	const int percent = chimera_godot_drawing_percent();
	const Size2i want(p_size.x * percent / 100, p_size.y * percent / 100);
	return Size2i(CLAMP(want.x, 16, p_max.x), CLAMP(want.y, 16, p_max.y));
}

bool DisplayServerChimera::_make_surface(const Size2i &p_size) {
	uint8_t *surface = (uint8_t *)memalloc((size_t)p_size.x * p_size.y * 4);
	memset(surface, 0, (size_t)p_size.x * p_size.y * 4);
	if (!OSMesaMakeCurrent((OSMesaContext)gl_context, surface, GL_UNSIGNED_BYTE, p_size.x, p_size.y)) {
		memfree(surface);
		return false;
	}
	if (gl_surface) {
		memfree(gl_surface);
	}
	gl_surface = surface;
	size = p_size;
	picture.resize((int64_t)size.x * size.y);
	memset(picture.ptrw(), 0, (size_t)picture.size() * 4);
	return true;
}

DisplayServerChimera::DisplayServerChimera(const Size2i &p_size, const Size2i &p_max_size, Error &r_error) {
	r_error = ERR_UNAVAILABLE;
	max_size = p_max_size;
	Input::get_singleton()->set_event_dispatch_function(_dispatch_input_events);

	// OpenGL 3.3, core profile: what the Compatibility renderer asks of a
	// desktop. Depth and stencil as a desktop window would have them.
	static const int attribs[] = {
		OSMESA_FORMAT, OSMESA_BGRA,
		OSMESA_DEPTH_BITS, 24,
		OSMESA_STENCIL_BITS, 8,
		OSMESA_ACCUM_BITS, 0,
		OSMESA_PROFILE, OSMESA_CORE_PROFILE,
		OSMESA_CONTEXT_MAJOR_VERSION, 3,
		OSMESA_CONTEXT_MINOR_VERSION, 3,
		0
	};
	gl_context = OSMesaCreateContextAttribs(attribs, nullptr);
	ERR_FAIL_NULL_MSG(gl_context, "OSMesa refused an OpenGL 3.3 core context.");
	ERR_FAIL_COND_MSG(!_make_surface(_clamp_size(p_size, max_size)), "OSMesa refused the window's surface.");

	glad_eglGetProcAddress = (PFNEGLGETPROCADDRESSPROC)_osmesa_get_proc;
	RasterizerGLES3::make_current(true);
	r_error = OK;
}

DisplayServerChimera::~DisplayServerChimera() {
	// The context goes with the machine; nothing is freed while Godot may
	// still be drawing.
}

bool DisplayServerChimera::has_feature(DisplayServerEnums::Feature p_feature) const {
	switch (p_feature) {
		case DisplayServerEnums::FEATURE_MOUSE:
		case DisplayServerEnums::FEATURE_MOUSE_WARP:
		case DisplayServerEnums::FEATURE_CLIPBOARD:
		case DisplayServerEnums::FEATURE_CLIPBOARD_PRIMARY:
			return true;
		default:
			return false;
	}
}

float DisplayServerChimera::screen_get_refresh_rate(int p_screen) const {
	OS_Chimera *os = OS_Chimera::get_chimera_singleton();
	return os ? (float)os->get_fps() : 60.0f;
}

Vector<DisplayServerEnums::WindowID> DisplayServerChimera::get_window_list() const {
	Vector<DisplayServerEnums::WindowID> list;
	list.push_back(DisplayServerEnums::MAIN_WINDOW_ID);
	return list;
}

void DisplayServerChimera::window_set_size(const Size2i p_size, DisplayServerEnums::WindowID p_window) {
	// the size the game was given, asked for again, is kept: the drawing
	// size's share is taken of what the game asks, not of what it was given
	if (p_size == size) {
		return;
	}
	Size2i want = _clamp_size(p_size, max_size);
	if (want == size) {
		return;
	}
	ERR_FAIL_COND_MSG(!_make_surface(want), "OSMesa refused the window's new size.");
	if (rect_changed_callback.is_valid()) {
		rect_changed_callback.call(Rect2i(Point2i(), size));
	}
}

void DisplayServerChimera::window_set_flag(DisplayServerEnums::WindowFlags p_flag, bool p_enabled, DisplayServerEnums::WindowID p_window) {
	ERR_FAIL_INDEX(p_flag, DisplayServerEnums::WINDOW_FLAG_MAX);
	window_flags[p_flag] = p_enabled;
}

bool DisplayServerChimera::window_get_flag(DisplayServerEnums::WindowFlags p_flag, DisplayServerEnums::WindowID p_window) const {
	ERR_FAIL_INDEX_V(p_flag, DisplayServerEnums::WINDOW_FLAG_MAX, false);
	return window_flags[p_flag];
}

BitField<MouseButtonMask> DisplayServerChimera::mouse_get_button_state() const {
	return Input::get_singleton()->get_mouse_button_mask();
}

void DisplayServerChimera::_dispatch_input_events(const Ref<InputEvent> &p_event) {
	DisplayServerChimera *ds = get_chimera_singleton();
	if (ds && ds->input_event_callback.is_valid()) {
		ds->input_event_callback.call(p_event);
	}
}

void DisplayServerChimera::process_events() {
	// A desktop tells a new window the mouse is over it and it has focus;
	// GUI hovering waits for the first of these.
	if (!told_mouse_entered && window_event_callback.is_valid()) {
		told_mouse_entered = true;
		window_event_callback.call(Variant(int(DisplayServerEnums::WINDOW_EVENT_FOCUS_IN)));
		window_event_callback.call(Variant(int(DisplayServerEnums::WINDOW_EVENT_MOUSE_ENTER)));
	}
	Input::get_singleton()->flush_buffered_events();
}

// The frame is finished: read it back, top row first. Reading leaves
// OpenGL's state as Godot set it.
void DisplayServerChimera::swap_buffers() {
	if (!capture) {
		return;
	}
	GLint read_fb = 0, pack_buffer = 0, pack_alignment = 4, pack_row_length = 0;
	glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &read_fb);
	glGetIntegerv(GL_PIXEL_PACK_BUFFER_BINDING, &pack_buffer);
	glGetIntegerv(GL_PACK_ALIGNMENT, &pack_alignment);
	glGetIntegerv(GL_PACK_ROW_LENGTH, &pack_row_length);
	glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
	glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
	glPixelStorei(GL_PACK_ALIGNMENT, 4);
	glPixelStorei(GL_PACK_ROW_LENGTH, 0);
	glReadPixels(0, 0, size.x, size.y, GL_BGRA, GL_UNSIGNED_BYTE, picture.ptrw());
	glPixelStorei(GL_PACK_ROW_LENGTH, pack_row_length);
	glPixelStorei(GL_PACK_ALIGNMENT, pack_alignment);
	glBindBuffer(GL_PIXEL_PACK_BUFFER, pack_buffer);
	glBindFramebuffer(GL_READ_FRAMEBUFFER, read_fb);

	// OpenGL's rows run bottom to top; the picture is opaque
	uint32_t *p = picture.ptrw();
	const int w = size.x, h = size.y;
	for (int y = 0; y < h / 2; y++) {
		uint32_t *a = p + (int64_t)y * w;
		uint32_t *b = p + (int64_t)(h - 1 - y) * w;
		for (int x = 0; x < w; x++) {
			uint32_t t = a[x];
			a[x] = b[x] | 0xFF000000u;
			b[x] = t | 0xFF000000u;
		}
	}
	if (h & 1) {
		uint32_t *m = p + (int64_t)(h / 2) * w;
		for (int x = 0; x < w; x++) {
			m[x] |= 0xFF000000u;
		}
	}
}

const uint32_t *DisplayServerChimera::get_picture(int &r_width, int &r_height) const {
	r_width = size.x;
	r_height = size.y;
	return picture.ptr();
}
