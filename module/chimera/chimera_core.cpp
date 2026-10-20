/**************************************************************************/
/*  chimera_core.cpp                                                      */
/**************************************************************************/
/* The core's calls: what Chimera asks of the machine, and the machine's  */
/* frame.                                                                 */
/*                                                                        */
/* Init starts Godot on the game's pack the way an exported game starts:  */
/* the pack is the main pack, the window is the core's (chimera_display), */
/* the sound is mixed when the core asks for it (Godot's own "Dummy"      */
/* driver, without its thread, as Godot's Movie Maker mode does), and     */
/* time goes by in whole frames (--fixed-fps, as Movie Maker mode again). */
/*                                                                        */
/* A frame: the inputs Chimera set become Godot's input events - keys,    */
/* mouse and one game controller - then the clock moves one frame on,     */
/* Godot runs one iteration of its main loop (physics, scripts, drawing), */
/* and the sound of exactly that frame is mixed.                          */
/**************************************************************************/

#include "chimera_compat.h"
#include "chimera_display.h"
#include "chimera_fs.h"
#include "chimera_os.h"

#include "core/config/engine.h"
#include "core/config/project_settings.h"
#include "core/input/input.h"
#include "core/os/main_loop.h"
#include "main/main.h"
#include "modules/zip/zip_reader.h"
#include "servers/audio/audio_driver_dummy.h"

#include <emulibc.h>

#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

extern "C" {
// chimera_wbx.c: the project's settings and files, through the guest kit
int chimera_setting_str(const char *p_key, char *r_out, int p_outsz);
long chimera_setting_long(const char *p_key, long p_default);
int chimera_slot_count(const char *p_id);
const char *chimera_slot_name(const char *p_id, int p_index, char *r_out, int p_outsz);
}

/* ---- the inputs --------------------------------------------------------- */

namespace {

enum InputKind {
	IN_MOUSE,
	IN_WHEEL,
	IN_KEY,
	IN_PAD,
};

struct InputDef {
	InputKind kind;
	int code; // MouseButton, Key or JoyButton
	KeyLocation location;
	char32_t lower; // the character typed, or 0
	char32_t upper; // with Shift
};

#define K(c, lo, up) { IN_KEY, (int)Key::c, KeyLocation::UNSPECIFIED, lo, up }
#define KL(c, loc) { IN_KEY, (int)Key::c, KeyLocation::loc, 0, 0 }

// In waterbox.config's order.
const InputDef inputs[] = {
	{ IN_MOUSE, (int)MouseButton::LEFT, KeyLocation::UNSPECIFIED, 0, 0 },
	{ IN_MOUSE, (int)MouseButton::MIDDLE, KeyLocation::UNSPECIFIED, 0, 0 },
	{ IN_MOUSE, (int)MouseButton::RIGHT, KeyLocation::UNSPECIFIED, 0, 0 },
	K(A, 'a', 'A'), K(B, 'b', 'B'), K(C, 'c', 'C'), K(D, 'd', 'D'), K(E, 'e', 'E'),
	K(F, 'f', 'F'), K(G, 'g', 'G'), K(H, 'h', 'H'), K(I, 'i', 'I'), K(J, 'j', 'J'),
	K(K, 'k', 'K'), K(L, 'l', 'L'), K(M, 'm', 'M'), K(N, 'n', 'N'), K(O, 'o', 'O'),
	K(P, 'p', 'P'), K(Q, 'q', 'Q'), K(R, 'r', 'R'), K(S, 's', 'S'), K(T, 't', 'T'),
	K(U, 'u', 'U'), K(V, 'v', 'V'), K(W, 'w', 'W'), K(X, 'x', 'X'), K(Y, 'y', 'Y'),
	K(Z, 'z', 'Z'),
	K(KEY_1, '1', '!'), K(KEY_2, '2', '@'), K(KEY_3, '3', '#'), K(KEY_4, '4', '$'),
	K(KEY_5, '5', '%'), K(KEY_6, '6', '^'), K(KEY_7, '7', '&'), K(KEY_8, '8', '*'),
	K(KEY_9, '9', '('), K(KEY_0, '0', ')'),
	K(SPACE, ' ', ' '), K(ENTER, 0, 0), K(ESCAPE, 0, 0), K(TAB, 0, 0), K(BACKSPACE, 0, 0),
	K(KEY_DELETE, 0, 0), K(INSERT, 0, 0), K(HOME, 0, 0), K(END, 0, 0),
	K(PAGEUP, 0, 0), K(PAGEDOWN, 0, 0),
	K(UP, 0, 0), K(DOWN, 0, 0), K(LEFT, 0, 0), K(RIGHT, 0, 0),
	KL(SHIFT, LEFT), KL(SHIFT, RIGHT), KL(CTRL, LEFT), KL(CTRL, RIGHT), KL(ALT, LEFT), KL(ALT, RIGHT),
	K(CAPSLOCK, 0, 0), K(NUMLOCK, 0, 0), K(SCROLLLOCK, 0, 0), K(PAUSE, 0, 0),
	K(F1, 0, 0), K(F2, 0, 0), K(F3, 0, 0), K(F4, 0, 0), K(F5, 0, 0), K(F6, 0, 0),
	K(F7, 0, 0), K(F8, 0, 0), K(F9, 0, 0), K(F10, 0, 0), K(F11, 0, 0), K(F12, 0, 0),
	K(MINUS, '-', '_'), K(EQUAL, '=', '+'), K(BRACKETLEFT, '[', '{'), K(BRACKETRIGHT, ']', '}'),
	K(BACKSLASH, '\\', '|'), K(SEMICOLON, ';', ':'), K(APOSTROPHE, '\'', '"'), K(COMMA, ',', '<'),
	K(PERIOD, '.', '>'), K(SLASH, '/', '?'), K(QUOTELEFT, '`', '~'),
	K(KP_0, '0', '0'), K(KP_1, '1', '1'), K(KP_2, '2', '2'), K(KP_3, '3', '3'), K(KP_4, '4', '4'),
	K(KP_5, '5', '5'), K(KP_6, '6', '6'), K(KP_7, '7', '7'), K(KP_8, '8', '8'), K(KP_9, '9', '9'),
	K(KP_ADD, '+', '+'), K(KP_SUBTRACT, '-', '-'), K(KP_MULTIPLY, '*', '*'), K(KP_DIVIDE, '/', '/'),
	K(KP_PERIOD, '.', '.'), K(KP_ENTER, 0, 0),
	{ IN_WHEEL, (int)MouseButton::WHEEL_UP, KeyLocation::UNSPECIFIED, 0, 0 },
	{ IN_WHEEL, (int)MouseButton::WHEEL_DOWN, KeyLocation::UNSPECIFIED, 0, 0 },
	{ IN_MOUSE, (int)MouseButton::MB_XBUTTON1, KeyLocation::UNSPECIFIED, 0, 0 },
	{ IN_MOUSE, (int)MouseButton::MB_XBUTTON2, KeyLocation::UNSPECIFIED, 0, 0 },
	{ IN_PAD, (int)JoyButton::A, KeyLocation::UNSPECIFIED, 0, 0 },
	{ IN_PAD, (int)JoyButton::B, KeyLocation::UNSPECIFIED, 0, 0 },
	{ IN_PAD, (int)JoyButton::X, KeyLocation::UNSPECIFIED, 0, 0 },
	{ IN_PAD, (int)JoyButton::Y, KeyLocation::UNSPECIFIED, 0, 0 },
	{ IN_PAD, (int)JoyButton::BACK, KeyLocation::UNSPECIFIED, 0, 0 },
	{ IN_PAD, (int)JoyButton::GUIDE, KeyLocation::UNSPECIFIED, 0, 0 },
	{ IN_PAD, (int)JoyButton::START, KeyLocation::UNSPECIFIED, 0, 0 },
	{ IN_PAD, (int)JoyButton::LEFT_STICK, KeyLocation::UNSPECIFIED, 0, 0 },
	{ IN_PAD, (int)JoyButton::RIGHT_STICK, KeyLocation::UNSPECIFIED, 0, 0 },
	{ IN_PAD, (int)JoyButton::LEFT_SHOULDER, KeyLocation::UNSPECIFIED, 0, 0 },
	{ IN_PAD, (int)JoyButton::RIGHT_SHOULDER, KeyLocation::UNSPECIFIED, 0, 0 },
	{ IN_PAD, (int)JoyButton::DPAD_UP, KeyLocation::UNSPECIFIED, 0, 0 },
	{ IN_PAD, (int)JoyButton::DPAD_DOWN, KeyLocation::UNSPECIFIED, 0, 0 },
	{ IN_PAD, (int)JoyButton::DPAD_LEFT, KeyLocation::UNSPECIFIED, 0, 0 },
	{ IN_PAD, (int)JoyButton::DPAD_RIGHT, KeyLocation::UNSPECIFIED, 0, 0 },
};

#undef K
#undef KL

const int INPUT_COUNT = sizeof(inputs) / sizeof(inputs[0]);

// Axes, in waterbox.config's order: the mouse's place in the window
// (0..65535 across it), then the controller's sticks (-32768..32767) and
// triggers (0..32767).
enum {
	AXIS_MOUSE_X,
	AXIS_MOUSE_Y,
	AXIS_LEFT_X,
	AXIS_LEFT_Y,
	AXIS_RIGHT_X,
	AXIS_RIGHT_Y,
	AXIS_TRIGGER_LEFT,
	AXIS_TRIGGER_RIGHT,
	AXIS_COUNT,
};
const int32_t axis_neutral[AXIS_COUNT] = { 32768, 32768, 0, 0, 0, 0, 0, 0 };
const JoyAxis pad_axis[AXIS_COUNT] = { JoyAxis::INVALID, JoyAxis::INVALID, JoyAxis::LEFT_X, JoyAxis::LEFT_Y,
	JoyAxis::RIGHT_X, JoyAxis::RIGHT_Y, JoyAxis::TRIGGER_LEFT, JoyAxis::TRIGGER_RIGHT };

const int MIX_RATE = 44100;

struct Machine {
	OS_Chimera *os = nullptr;
	bool started = false;

	uint8_t buttons[INPUT_COUNT] = {};
	uint8_t held[INPUT_COUNT] = {};
	int32_t axes[AXIS_COUNT] = {};
	int32_t sent_axes[AXIS_COUNT] = {};
	Point2i mouse;
	bool mouse_known = false;

	// double clicks, as a desktop counts them: the same button again within
	// 400 ms and 5 pixels
	uint64_t last_click_msec = 0;
	MouseButton last_click_button = MouseButton::NONE;
	Point2i last_click_pos;

	// the sound of one frame
	Vector<int32_t> mix;
	Vector<int16_t> audio;
	int audio_frames = 0;
};

Machine *machine = nullptr;
char load_error[1024];
bool render_enabled = true;
int max_picture_w = 1920;
int max_picture_h = 1080;

} // namespace

extern "C" void chimera_godot_max_picture(int *r_width, int *r_height) {
	*r_width = max_picture_w;
	*r_height = max_picture_h;
}

static BitField<MouseButtonMask> _mouse_mask() {
	BitField<MouseButtonMask> mask = MouseButtonMask::NONE;
	for (int i = 0; i < INPUT_COUNT; i++) {
		if (inputs[i].kind == IN_MOUSE && machine->held[i]) {
			mask.set_flag(mouse_button_to_mask((MouseButton)inputs[i].code));
		}
	}
	return mask;
}

static bool _held(Key p_key) {
	for (int i = 0; i < INPUT_COUNT; i++) {
		if (inputs[i].kind == IN_KEY && inputs[i].code == (int)p_key && machine->held[i]) {
			return true;
		}
	}
	return false;
}

static void _modifiers(const Ref<InputEventWithModifiers> &p_ev) {
	p_ev->set_shift_pressed(_held(Key::SHIFT));
	p_ev->set_ctrl_pressed(_held(Key::CTRL));
	p_ev->set_alt_pressed(_held(Key::ALT));
}

static void _send_key(const InputDef &p_def, bool p_pressed) {
	Ref<InputEventKey> k;
	k.instantiate();
	k->set_window_id(DisplayServerEnums::MAIN_WINDOW_ID);
	_modifiers(k);
	k->set_pressed(p_pressed);
	k->set_keycode((Key)p_def.code);
	k->set_physical_keycode((Key)p_def.code);
	k->set_key_label((Key)p_def.code);
	k->set_location(p_def.location);
	if (p_pressed && p_def.lower && !k->is_ctrl_pressed() && !k->is_alt_pressed()) {
		bool shift = k->is_shift_pressed();
		bool letter = p_def.lower >= 'a' && p_def.lower <= 'z';
		if (letter && _held(Key::CAPSLOCK)) {
			shift = !shift;
		}
		k->set_unicode(shift ? p_def.upper : p_def.lower);
	}
	Input::get_singleton()->parse_input_event(k);
}

static void _send_mouse_button(MouseButton p_button, bool p_pressed, float p_factor) {
	Ref<InputEventMouseButton> mb;
	mb.instantiate();
	mb->set_window_id(DisplayServerEnums::MAIN_WINDOW_ID);
	_modifiers(mb);
	mb->set_button_index(p_button);
	mb->set_pressed(p_pressed);
	mb->set_position(machine->mouse);
	mb->set_global_position(machine->mouse);
	mb->set_button_mask(_mouse_mask());
	mb->set_factor(p_factor);
	if (p_pressed && p_button != MouseButton::WHEEL_UP && p_button != MouseButton::WHEEL_DOWN) {
		uint64_t now = OS::get_singleton()->get_ticks_msec();
		if (machine->last_click_button == p_button && now - machine->last_click_msec < 400 && machine->mouse.distance_to(machine->last_click_pos) < 5) {
			machine->last_click_msec = 0;
			machine->last_click_button = MouseButton::NONE;
			mb->set_double_click(true);
		} else {
			machine->last_click_msec = now;
			machine->last_click_button = p_button;
			machine->last_click_pos = machine->mouse;
		}
	}
	Input::get_singleton()->parse_input_event(mb);
}

static bool _is_modifier(const InputDef &p_def) {
	return p_def.kind == IN_KEY && (p_def.code == (int)Key::SHIFT || p_def.code == (int)Key::CTRL || p_def.code == (int)Key::ALT);
}

// The inputs of this frame, as the events a desktop would have sent since
// the last one: the mouse moves first, then whatever was let go, then
// whatever was pressed, each in the order of the list - except that Shift,
// Ctrl and Alt go down before the other keys of the same frame and come up
// after them, as fingers do, so that Shift and A pressed together type "A".
static void _send_inputs() {
	DisplayServerChimera *ds = DisplayServerChimera::get_chimera_singleton();
	int w = 1, h = 1;
	ds->get_picture(w, h);
	Point2i pos(
			(int)((int64_t)CLAMP(machine->axes[AXIS_MOUSE_X], 0, 65535) * w / 65536),
			(int)((int64_t)CLAMP(machine->axes[AXIS_MOUSE_Y], 0, 65535) * h / 65536));
	if (!machine->mouse_known || pos != machine->mouse) {
		Point2i rel = machine->mouse_known ? pos - machine->mouse : Point2i();
		machine->mouse = pos;
		machine->mouse_known = true;
		ds->set_mouse_position_from_core(pos);
		Ref<InputEventMouseMotion> mm;
		mm.instantiate();
		mm->set_window_id(DisplayServerEnums::MAIN_WINDOW_ID);
		_modifiers(mm);
		mm->set_position(pos);
		mm->set_global_position(pos);
		mm->set_relative(rel);
		mm->set_relative_screen_position(rel);
		mm->set_button_mask(_mouse_mask());
		Input::get_singleton()->parse_input_event(mm);
	}

	// 0: releases of the others, 1: releases of the modifiers,
	// 2: presses of the modifiers, 3: presses of the others
	for (int pass = 0; pass < 4; pass++) {
		bool press = pass >= 2;
		bool modifiers = pass == 1 || pass == 2;
		for (int i = 0; i < INPUT_COUNT; i++) {
			const InputDef &def = inputs[i];
			if (_is_modifier(def) != modifiers) {
				continue;
			}
			bool want = machine->buttons[i] != 0;
			if (def.kind == IN_WHEEL) {
				// a wheel turns one notch for each frame it is held
				if (press && want) {
					_send_mouse_button((MouseButton)def.code, true, 1.0f);
					_send_mouse_button((MouseButton)def.code, false, 1.0f);
				}
				continue;
			}
			if (want == (machine->held[i] != 0) || want != press) {
				continue;
			}
			machine->held[i] = want;
			switch (def.kind) {
				case IN_KEY:
					_send_key(def, want);
					break;
				case IN_MOUSE:
					_send_mouse_button((MouseButton)def.code, want, 1.0f);
					break;
				case IN_PAD:
					Input::get_singleton()->joy_button(0, (JoyButton)def.code, want);
					break;
				default:
					break;
			}
		}
	}

	for (int a = AXIS_LEFT_X; a < AXIS_COUNT; a++) {
		if (machine->axes[a] == machine->sent_axes[a]) {
			continue;
		}
		machine->sent_axes[a] = machine->axes[a];
		float v = (float)CLAMP(machine->axes[a], -32767, 32767) / 32767.0f;
		if (a >= AXIS_TRIGGER_LEFT) {
			v = MAX(v, 0.0f);
		}
		Input::get_singleton()->joy_axis(0, pad_axis[a], v);
	}
}

/* ---- the files ---------------------------------------------------------- */

// What the project brought, by the name Chimera mounted it under, kept as
// the name the game sees in /game. A name with a folder in it keeps only
// its last part: the sandbox's files have no folders.
static String _mounted_name(const char *p_name) {
	String s = String::utf8(p_name);
	return s.get_file();
}

// The name Chimera mounted the pack under: the project's "pack" slot, or,
// for a file opened without a project, the name rom.name gives (the file is
// also mounted as "game", waterbox.config's romFile).
static String _pack_host_name() {
	char name[512];
	if (chimera_slot_count("pack") > 0 && chimera_slot_name("pack", 0, name, sizeof name)) {
		return String::utf8(name);
	}
	FILE *f = fopen("rom.name", "rb");
	if (f) {
		size_t got = fread(name, 1, sizeof name - 1, f);
		fclose(f);
		name[got] = 0;
		if (got) {
			return String::utf8(name);
		}
	}
	return String("game");
}

/* ---- which Godot made the game ------------------------------------------- */

// The version a pack's header says it was exported with: at its start for a
// .pck, or at the end of the executable it was embedded in, as Godot itself
// looks (core/io/file_access_pack.cpp). False when it is not a Godot pack.
static bool _pack_version(const ChimeraFSRef &p_pack, uint32_t &r_major, uint32_t &r_minor, uint32_t &r_patch) {
	if (p_pack.is_null() || !p_pack->is_mounted()) {
		return false;
	}
	const int fd = p_pack->mount_fd;
	const uint64_t len = p_pack->mount_size;
	auto read_at = [fd](uint64_t p_at, void *r_buf, size_t p_size) {
		return ::pread(fd, r_buf, p_size, (off_t)p_at) == (ssize_t)p_size;
	};
	const uint32_t magic_gdpc = 0x43504447;
	uint64_t start = 0;
	uint32_t magic = 0;
	if (!read_at(0, &magic, 4) || magic != magic_gdpc) {
		// embedded: ... the pack, its size (64 bits), the magic again
		uint64_t size = 0;
		if (len < 12 || !read_at(len - 4, &magic, 4) || magic != magic_gdpc || !read_at(len - 12, &size, 8) || size + 12 > len) {
			return false;
		}
		start = len - 12 - size;
		if (!read_at(start, &magic, 4) || magic != magic_gdpc) {
			return false;
		}
	}
	uint32_t header[4];
	if (!read_at(start + 4, header, sizeof header)) {
		return false;
	}
	r_major = header[1];
	r_minor = header[2];
	r_patch = header[3];
	return true;
}

// Refuses a game this core's Godot is not the one for, saying which is.
static bool _check_pack_version(const ChimeraFSRef &p_pack) {
	uint32_t major = 0, minor = 0, patch = 0;
	if (!_pack_version(p_pack, major, minor, patch)) {
		return true; // not a pack Godot knows; Godot says so itself
	}
	if (major == 4 && minor >= CHIMERA_ACCEPTS_MINOR_FROM && minor <= CHIMERA_GODOT_MINOR) {
		return true;
	}
	if (major != 4) {
		snprintf(load_error, sizeof load_error, "this game was made with Godot %u.%u.%u; the Godot cores run games of Godot 4 (4.%d to 4.%d)",
				major, minor, patch, CHIMERA_OLDEST_CORE_MINOR, CHIMERA_NEWEST_CORE_MINOR);
	} else if (minor > CHIMERA_NEWEST_CORE_MINOR) {
		snprintf(load_error, sizeof load_error, "this game was made with Godot %u.%u.%u, newer than every Godot core (the newest runs Godot 4.%d games)",
				major, minor, patch, CHIMERA_NEWEST_CORE_MINOR);
	} else if (minor < CHIMERA_OLDEST_CORE_MINOR) {
		snprintf(load_error, sizeof load_error, "this game was made with Godot %u.%u.%u: open it with the Godot 4.%d core, which also runs games of older Godot 4 versions",
				major, minor, patch, CHIMERA_OLDEST_CORE_MINOR);
	} else {
		snprintf(load_error, sizeof load_error, "this game was made with Godot %u.%u.%u: open it with the Godot 4.%u core (this one runs Godot 4.%d games)",
				major, minor, patch, minor, CHIMERA_GODOT_MINOR);
	}
	return false;
}

/* ---- save data ---------------------------------------------------------- */

// A whole file Chimera mounted, read into memory (save files are small).
static bool _read_mounted(const char *p_name, Vector<uint8_t> &r_data) {
	int fd = ::open(p_name, O_RDONLY);
	if (fd < 0) {
		return false;
	}
	off_t size = ::lseek(fd, 0, SEEK_END);
	bool ok = size >= 0;
	if (ok) {
		r_data.resize(size);
		ok = size == 0 || ::pread(fd, r_data.ptrw(), (size_t)size, 0) == (ssize_t)size;
	}
	::close(fd);
	return ok;
}

// A save file name as Export Save Data wrote it: relative, '/' between
// folders, nothing that climbs out.
static bool _good_save_name(const String &p_name) {
	if (p_name.is_empty() || p_name.begins_with("/") || p_name.contains_char('\\')) {
		return false;
	}
	for (const String &part : p_name.split("/")) {
		if (part.is_empty() || part == "." || part == "..") {
			return false;
		}
	}
	return true;
}

// The project's save data, put in user:// before the game starts, as Export
// Save Data wrote it: each file under its own name, and a .zip (what the
// export writes for several files) unpacked with its folders.
static bool _seed_save_data() {
	int count = chimera_slot_count("savedata");
	for (int i = 0; i < count; i++) {
		char name[512];
		if (!chimera_slot_name("savedata", i, name, sizeof name)) {
			continue;
		}
		Vector<uint8_t> data;
		if (!_read_mounted(name, data)) {
			snprintf(load_error, sizeof load_error, "the save data file %s could not be read", name);
			return false;
		}
		String file = _mounted_name(name);
		if (file.get_extension().to_lower() != "zip") {
			ChimeraFS::put("/user/" + file, data.ptr(), data.size());
			continue;
		}
		ChimeraFS::put("/tmp/chimera-savedata.zip", data.ptr(), data.size());
		Ref<ZIPReader> zip;
		zip.instantiate();
		if (zip->open("/tmp/chimera-savedata.zip") != OK) {
			snprintf(load_error, sizeof load_error, "the save data file %s is not a zip Godot can read", name);
			return false;
		}
		PackedStringArray entries = zip->get_files();
		for (const String &entry : entries) {
			if (entry.ends_with("/")) {
				continue; // a folder: made with the files in it
			}
			if (!_good_save_name(entry)) {
				snprintf(load_error, sizeof load_error, "the save data zip %s holds %s, which is not a name in user://", name, entry.utf8().get_data());
				return false;
			}
			PackedByteArray bytes = zip->read_file(entry, true);
			ChimeraFS::put("/user/" + entry, bytes.ptr(), bytes.size());
		}
		zip->close();
		ChimeraFS::remove("/tmp/chimera-savedata.zip");
	}
	return true;
}

/* ---- Chimera's calls ---------------------------------------------------- */

ECL_EXPORT const char *GetLoadError(void) {
	return load_error;
}

ECL_EXPORT int Init(void) {
	load_error[0] = 0;
	if (machine) {
		snprintf(load_error, sizeof load_error, "the machine is already running");
		return 0;
	}
	machine = memnew(Machine);
	for (int a = 0; a < AXIS_COUNT; a++) {
		machine->axes[a] = axis_neutral[a];
		machine->sent_axes[a] = axis_neutral[a];
	}

	long fps = chimera_setting_long("frameRate", 60);
	if (fps < 1 || fps > 1000) {
		fps = 60;
	}
	// 2000-01-01 00:00:00 UTC unless the project says otherwise
	long start_time = chimera_setting_long("startTime", 946684800L);

	machine->os = new OS_Chimera();
	machine->os->set_clock((uint32_t)fps, start_time);

	// The disk is made now, before Godot starts, so that the pack is on it
	// when Godot looks; OS_Chimera::initialize() only hands it to Godot.
	ChimeraFS::initialize();

	// An exported game finds its pack the way Godot's own export templates
	// do (this core is one): beside its executable, as <name>.pck, or inside
	// the executable when the pack was embedded in it. So the pack goes in
	// /game under its own name, and the executable Godot is told it is has
	// the same name without ".pck" - or is the file itself, when what the
	// project brought is a game's executable with its pack inside.
	String pack_host = _pack_host_name();
	String pack_name = pack_host == "game" ? String("game.pck") : _mounted_name(pack_host.utf8().get_data());
	String pack_path = "/game/" + pack_name;
	if (!ChimeraFS::mount(pack_path, pack_host.utf8().get_data())) {
		snprintf(load_error, sizeof load_error, "the game's pack (%s) could not be opened", pack_host.utf8().get_data());
		return 0;
	}
	if (!_check_pack_version(ChimeraFS::find(pack_path))) {
		return 0;
	}
	if (pack_name.get_extension().to_lower() == "pck") {
		machine->os->set_executable_path("/game/" + pack_name.get_basename());
	} else {
		machine->os->set_executable_path(pack_path);
	}
	int extra = chimera_slot_count("files");
	for (int i = 0; i < extra; i++) {
		char name[512];
		if (chimera_slot_name("files", i, name, sizeof name)) {
			ChimeraFS::mount("/game/" + _mounted_name(name), name);
		}
	}

	CharString fps_arg = String::num_int64(fps).utf8();
	char verbose[8] = "";
	chimera_setting_str("verbose", verbose, sizeof verbose);
	const char *args[] = {
		"--display-driver", "chimera",
		"--rendering-driver", "opengl3",
		"--audio-driver", "Dummy",
		"--fixed-fps", fps_arg.get_data(),
		"--disable-crash-handler",
		// Godot's log file, which a desktop game keeps in user://logs, goes
		// nowhere: it would grow in the machine's memory and come out with
		// the save data. Its lines still reach the console.
		"--log-file", "/dev/null",
		strcmp(verbose, "true") == 0 ? "--verbose" : "--no-header",
	};
	const int argc = sizeof args / sizeof args[0];
	char *argv[argc + 1];
	for (int i = 0; i < argc; i++) {
		argv[i] = (char *)args[i];
	}
	argv[argc] = nullptr;

	Error err = Main::setup(machine->os->get_executable_path().utf8().get_data(), argc, argv, false);
	if (err != OK) {
		snprintf(load_error, sizeof load_error, "Godot could not start this game (setup error %d); is this a Godot 4 game's .pck?", (int)err);
		return 0;
	}

	// The OpenGL renderer's shader cache, which a desktop game keeps in
	// user://shader_cache, is not kept: it would come out with the save data
	// and fill every savestate. Set here, after the game's settings are read
	// and before the renderer starts, so a game cannot turn it back on.
	ProjectSettings::get_singleton()->set_setting("rendering/shader_compiler/shader_cache/enabled", false);

	// the sound is mixed when the core asks, at the rate Chimera plays it
	AudioDriverDummy *dummy = AudioDriverDummy::get_dummy_singleton();
	dummy->set_use_threads(false);
	dummy->set_mix_rate(MIX_RATE);

	err = Main::setup2();
	if (err != OK) {
		snprintf(load_error, sizeof load_error, "Godot could not start this game (setup2 error %d)", (int)err);
		return 0;
	}
	// before the game's first scene, which may read its saves at once
	if (!_seed_save_data()) {
		return 0;
	}
	if (Main::start() != EXIT_SUCCESS) {
		snprintf(load_error, sizeof load_error, "Godot could not start this game: its main scene did not load");
		return 0;
	}
	OS::get_singleton()->get_main_loop()->initialize();
	machine->started = true;

	// one controller, connected from the start; its buttons arrive in
	// Godot's own numbering, so no mapping is looked up
	Dictionary info;
	info["mapping_handled"] = true;
	Input::get_singleton()->joy_connection_changed(0, true, "Chimera Controller", "", info);

	machine->mix.resize((int64_t)MIX_RATE * 2);
	machine->audio.resize((int64_t)MIX_RATE * 2);
	return 1;
}

ECL_EXPORT void SetButton(int32_t p_index, int32_t p_state) {
	if (machine && p_index >= 0 && p_index < INPUT_COUNT) {
		machine->buttons[p_index] = p_state ? 1 : 0;
	}
}

ECL_EXPORT void SetAxis(int32_t p_index, int32_t p_value) {
	if (machine && p_index >= 0 && p_index < AXIS_COUNT) {
		machine->axes[p_index] = p_value;
	}
}

ECL_EXPORT void SetRenderingEnabled(int p_on) {
	render_enabled = p_on != 0;
}

ECL_EXPORT void FrameAdvance(uint64_t p_packed) {
	(void)p_packed;
	if (!machine || !machine->started) {
		return;
	}
	DisplayServerChimera *ds = DisplayServerChimera::get_chimera_singleton();
	ds->set_capture(render_enabled);

	_send_inputs();
	ds->process_events();

	OS_Chimera *os = machine->os;
	uint64_t frame = os->get_frame_count();
	os->advance_frame();
	Main::iteration();

	// exactly this frame's share of the second: rate / fps samples, the
	// remainder carried so that a second holds the whole rate
	uint32_t fps = os->get_fps();
	int frames = (int)(((frame + 1) * MIX_RATE) / fps - (frame * MIX_RATE) / fps);
	AudioDriverDummy::get_dummy_singleton()->mix_audio(frames, machine->mix.ptrw());
	const int32_t *src = machine->mix.ptr();
	int16_t *dst = machine->audio.ptrw();
	for (int i = 0; i < frames * 2; i++) {
		dst[i] = (int16_t)(src[i] >> 16);
	}
	machine->audio_frames = frames;
}

ECL_EXPORT uint32_t *GetVideoBgra(void) {
	int w, h;
	DisplayServerChimera *ds = DisplayServerChimera::get_chimera_singleton();
	return ds ? (uint32_t *)ds->get_picture(w, h) : nullptr;
}

ECL_EXPORT int GetVideoWidth(void) {
	int w = 0, h = 0;
	DisplayServerChimera *ds = DisplayServerChimera::get_chimera_singleton();
	if (ds) {
		ds->get_picture(w, h);
	}
	return w;
}

ECL_EXPORT int GetVideoHeight(void) {
	int w = 0, h = 0;
	DisplayServerChimera *ds = DisplayServerChimera::get_chimera_singleton();
	if (ds) {
		ds->get_picture(w, h);
	}
	return h;
}

ECL_EXPORT int16_t *GetAudio(void) {
	return machine ? machine->audio.ptrw() : nullptr;
}

ECL_EXPORT int GetAudioSampleCount(void) {
	return machine ? machine->audio_frames : 0;
}

ECL_EXPORT int GetVsyncNumerator(void) {
	return machine && machine->os ? (int)machine->os->get_fps() : 60;
}

ECL_EXPORT int GetVsyncDenominator(void) {
	return 1;
}

// Godot reads its input every frame.
ECL_EXPORT int InputWasRead(void) {
	return 1;
}

ECL_EXPORT int GetMemoryDomainCount(void) {
	return 0;
}

ECL_EXPORT const char *GetMemoryDomainName(int p_index) {
	return "";
}

ECL_EXPORT uint8_t *GetMemoryDomainPtr(int p_index) {
	return nullptr;
}

ECL_EXPORT int64_t GetMemoryDomainSize(int p_index) {
	return 0;
}

ECL_EXPORT int GetMemoryDomainWritable(int p_index) {
	return 0;
}

// Emulator > Export Save Data: everything the game has written to user://,
// under its path there. The list is taken when Chimera asks for the count.
static Vector<String> savedata_paths;
static CharString savedata_name;

ECL_EXPORT int32_t GetSaveDataFileCount(void) {
	ChimeraFS::list_user_files(savedata_paths);
	return (int32_t)savedata_paths.size();
}

ECL_EXPORT const char *GetSaveDataFileName(int32_t p_index) {
	if (p_index < 0 || p_index >= savedata_paths.size()) {
		return "";
	}
	savedata_name = savedata_paths[p_index].utf8();
	return savedata_name.get_data();
}

ECL_EXPORT int64_t GetSaveDataFileSize(int32_t p_index) {
	if (p_index < 0 || p_index >= savedata_paths.size()) {
		return 0;
	}
	ChimeraFSRef n = ChimeraFS::find("/user/" + savedata_paths[p_index]);
	return n.is_valid() ? (int64_t)n->size() : 0;
}

ECL_EXPORT const uint8_t *GetSaveDataFileBuffer(int32_t p_index) {
	if (p_index < 0 || p_index >= savedata_paths.size()) {
		return nullptr;
	}
	ChimeraFSRef n = ChimeraFS::find("/user/" + savedata_paths[p_index]);
	return n.is_valid() && !n->is_mounted() ? n->data.ptr() : nullptr;
}

// The machine's own clock, in frames.
ECL_EXPORT uint64_t GetCycleCount(void) {
	return machine && machine->os ? machine->os->get_frame_count() : 0;
}
