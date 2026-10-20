/**************************************************************************/
/*  chimera_display.h                                                     */
/**************************************************************************/
/* The window Godot draws in inside the Chimera core.                     */
/*                                                                        */
/* There is no window: Godot's Compatibility renderer (OpenGL 3.3) draws  */
/* into Mesa's softpipe through OSMesa, an OpenGL that renders into       */
/* memory and is compiled into the core, and each finished frame is read  */
/* back into the picture Chimera shows. The "window" is the size the game */
/* asks for in its project settings, the one screen is that size too, and */
/* the mouse and keys arrive from Chimera's inputs (chimera_core.cpp)     */
/* instead of from a desktop.                                             */
/**************************************************************************/

#pragma once

#include "chimera_compat.h"

class DisplayServerChimera : public DisplayServerHeadless {
	GDSOFTCLASS(DisplayServerChimera, DisplayServerHeadless);

	static Vector<String> get_rendering_drivers_func();
	static DisplayServer *create_func(const String &p_rendering_driver, DisplayServerEnums::WindowMode p_mode, DisplayServerEnums::VSyncMode p_vsync_mode, uint32_t p_flags, const Vector2i *p_position, const Vector2i &p_resolution, int p_screen, DisplayServerEnums::Context p_context, int64_t p_parent_window, Error &r_error);

	static void _dispatch_input_events(const Ref<InputEvent> &p_event);

	void *gl_context = nullptr;
	uint8_t *gl_surface = nullptr;
	Size2i size;
	Size2i max_size;

	// The last frame drawn, top row first, as Chimera's BGRA.
	Vector<uint32_t> picture;
	bool capture = true;

	DisplayServerEnums::WindowMode window_mode = DisplayServerEnums::WINDOW_MODE_WINDOWED;
	DisplayServerEnums::VSyncMode vsync_mode = DisplayServerEnums::VSYNC_ENABLED;
	DisplayServerEnums::MouseMode mouse_mode = DisplayServerEnums::MOUSE_MODE_VISIBLE;
	DisplayServerEnums::CursorShape cursor_shape = DisplayServerEnums::CURSOR_ARROW;
	Point2i mouse_pos;
	String clipboard;
	String clipboard_primary;
	ObjectID window_instance;
	bool window_flags[DisplayServerEnums::WINDOW_FLAG_MAX] = {};

	Callable input_event_callback;
	Callable window_event_callback;
	Callable rect_changed_callback;
	bool told_mouse_entered = false;

	bool _make_surface(const Size2i &p_size);

public:
	static void register_chimera_driver();
	static DisplayServerChimera *get_chimera_singleton() { return static_cast<DisplayServerChimera *>(get_singleton()); }

	// For the core: the picture, and whether to read it back at all (turbo
	// skips the reading, never the drawing).
	const uint32_t *get_picture(int &r_width, int &r_height) const;
	void set_capture(bool p_capture) { capture = p_capture; }
	void set_mouse_position_from_core(const Point2i &p_pos) { mouse_pos = p_pos; }

	bool has_feature(DisplayServerEnums::Feature p_feature) const override;
	String get_name() const override { return "chimera"; }

	int get_screen_count() const override { return 1; }
	int get_primary_screen() const override { return 0; }
	Size2i screen_get_size(int p_screen = DisplayServerEnums::SCREEN_OF_MAIN_WINDOW) const override { return size; }
	Rect2i screen_get_usable_rect(int p_screen = DisplayServerEnums::SCREEN_OF_MAIN_WINDOW) const override { return Rect2i(Point2i(), size); }
	float screen_get_refresh_rate(int p_screen = DisplayServerEnums::SCREEN_OF_MAIN_WINDOW) const override;

	Vector<DisplayServerEnums::WindowID> get_window_list() const override;

	void window_attach_instance_id(ObjectID p_instance, DisplayServerEnums::WindowID p_window = DisplayServerEnums::MAIN_WINDOW_ID) override { window_instance = p_instance; }
	ObjectID window_get_attached_instance_id(DisplayServerEnums::WindowID p_window = DisplayServerEnums::MAIN_WINDOW_ID) const override { return window_instance; }

	void window_set_rect_changed_callback(const Callable &p_callable, DisplayServerEnums::WindowID p_window = DisplayServerEnums::MAIN_WINDOW_ID) override { rect_changed_callback = p_callable; }
	void window_set_window_event_callback(const Callable &p_callable, DisplayServerEnums::WindowID p_window = DisplayServerEnums::MAIN_WINDOW_ID) override { window_event_callback = p_callable; }
	void window_set_input_event_callback(const Callable &p_callable, DisplayServerEnums::WindowID p_window = DisplayServerEnums::MAIN_WINDOW_ID) override { input_event_callback = p_callable; }

	int window_get_current_screen(DisplayServerEnums::WindowID p_window = DisplayServerEnums::MAIN_WINDOW_ID) const override { return 0; }

	void window_set_size(const Size2i p_size, DisplayServerEnums::WindowID p_window = DisplayServerEnums::MAIN_WINDOW_ID) override;
	Size2i window_get_size(DisplayServerEnums::WindowID p_window = DisplayServerEnums::MAIN_WINDOW_ID) const override { return size; }
	Size2i window_get_size_with_decorations(DisplayServerEnums::WindowID p_window = DisplayServerEnums::MAIN_WINDOW_ID) const override { return size; }

	void window_set_mode(DisplayServerEnums::WindowMode p_mode, DisplayServerEnums::WindowID p_window = DisplayServerEnums::MAIN_WINDOW_ID) override { window_mode = p_mode; }
	DisplayServerEnums::WindowMode window_get_mode(DisplayServerEnums::WindowID p_window = DisplayServerEnums::MAIN_WINDOW_ID) const override { return window_mode; }
	void window_set_vsync_mode(DisplayServerEnums::VSyncMode p_vsync_mode, DisplayServerEnums::WindowID p_window = DisplayServerEnums::MAIN_WINDOW_ID) override { vsync_mode = p_vsync_mode; }
	DisplayServerEnums::VSyncMode window_get_vsync_mode(DisplayServerEnums::WindowID p_window) const override { return vsync_mode; }
	void window_set_flag(DisplayServerEnums::WindowFlags p_flag, bool p_enabled, DisplayServerEnums::WindowID p_window = DisplayServerEnums::MAIN_WINDOW_ID) override;
	bool window_get_flag(DisplayServerEnums::WindowFlags p_flag, DisplayServerEnums::WindowID p_window = DisplayServerEnums::MAIN_WINDOW_ID) const override;
	bool window_is_focused(DisplayServerEnums::WindowID p_window = DisplayServerEnums::MAIN_WINDOW_ID) const override { return true; }
	bool window_can_draw(DisplayServerEnums::WindowID p_window = DisplayServerEnums::MAIN_WINDOW_ID) const override { return true; }
	bool can_any_window_draw() const override { return true; }

	void process_events() override;
	void swap_buffers() override;

	void mouse_set_mode(DisplayServerEnums::MouseMode p_mode) override { mouse_mode = p_mode; }
	DisplayServerEnums::MouseMode mouse_get_mode() const override { return mouse_mode; }
	void warp_mouse(const Point2i &p_position) override { mouse_pos = p_position; }
	Point2i mouse_get_position() const override { return mouse_pos; }
	BitField<MouseButtonMask> mouse_get_button_state() const override;

	void clipboard_set(const String &p_text) override { clipboard = p_text; }
	String clipboard_get() const override { return clipboard; }
	void clipboard_set_primary(const String &p_text) override { clipboard_primary = p_text; }
	String clipboard_get_primary() const override { return clipboard_primary; }

	void cursor_set_shape(DisplayServerEnums::CursorShape p_shape) override { cursor_shape = p_shape; }
	DisplayServerEnums::CursorShape cursor_get_shape() const override { return cursor_shape; }

	DisplayServerChimera(const Size2i &p_size, const Size2i &p_max_size, Error &r_error);
	~DisplayServerChimera();
};
