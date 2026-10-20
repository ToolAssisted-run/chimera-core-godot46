/**************************************************************************/
/*  chimera_os.cpp                                                        */
/**************************************************************************/
/* See chimera_os.h.                                                      */
/**************************************************************************/

#include "chimera_os.h"

#include "chimera_display.h"
#include "chimera_fs.h"

#include "core/os/main_loop.h"

#include <time.h>

void OS_Chimera::set_clock(uint32_t p_fps, int64_t p_start_unix_time) {
	fps = p_fps ? p_fps : 60;
	start_unix_time = p_start_unix_time;
	frame_count = 0;
}

uint64_t OS_Chimera::get_ticks_usec() const {
	return frame_count * 1000000ull / fps;
}

double OS_Chimera::get_unix_time() const {
	return (double)start_unix_time + (double)frame_count / (double)fps;
}

OS::DateTime OS_Chimera::get_datetime(bool p_utc) const {
	// the machine's clock is in UTC, so local time is UTC as well
	time_t t = (time_t)(start_unix_time + (int64_t)(frame_count / fps));
	struct tm lt;
	gmtime_r(&t, &lt);
	DateTime ret;
	ret.year = 1900 + lt.tm_year;
	ret.month = (Month)(lt.tm_mon + 1);
	ret.day = lt.tm_mday;
	ret.weekday = (Weekday)lt.tm_wday;
	ret.hour = lt.tm_hour;
	ret.minute = lt.tm_min;
	ret.second = lt.tm_sec;
	ret.dst = false;
	return ret;
}

OS::TimeZoneInfo OS_Chimera::get_time_zone_info() const {
	TimeZoneInfo ret;
	ret.bias = 0;
	ret.name = "UTC";
	return ret;
}

// splitmix64: the same bytes on every run, carried by savestates with the
// rest of the OS object.
Error OS_Chimera::get_entropy(uint8_t *r_buffer, int p_bytes) {
	for (int i = 0; i < p_bytes; i++) {
		if ((i & 7) == 0) {
			entropy_state += 0x9E3779B97F4A7C15ull;
		}
		uint64_t z = entropy_state;
		z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
		z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
		z ^= z >> 31;
		r_buffer[i] = (uint8_t)(z >> ((i & 7) * 8));
	}
	return OK;
}

Dictionary OS_Chimera::get_memory_info() const {
	Dictionary meminfo;
	meminfo["physical"] = (int64_t)4 << 30;
	meminfo["free"] = (int64_t)2 << 30;
	meminfo["available"] = (int64_t)2 << 30;
	meminfo["stack"] = (int64_t)8 << 20;
	return meminfo;
}

String OS_Chimera::get_executable_path() const {
	return executable_path;
}

bool OS_Chimera::_check_internal_feature_support(const String &p_feature) {
	return p_feature == "pc" || p_feature == "linux";
}

void OS_Chimera::initialize() {
	OS_Unix::initialize_core();
	ChimeraFS::initialize();
	FileAccess::make_default<FileAccessChimera>(FileAccess::ACCESS_RESOURCES);
	FileAccess::make_default<FileAccessChimera>(FileAccess::ACCESS_USERDATA);
	FileAccess::make_default<FileAccessChimera>(FileAccess::ACCESS_FILESYSTEM);
	DirAccess::make_default<DirAccessChimera>(DirAccess::ACCESS_RESOURCES);
	DirAccess::make_default<DirAccessChimera>(DirAccess::ACCESS_USERDATA);
	DirAccess::make_default<DirAccessChimera>(DirAccess::ACCESS_FILESYSTEM);
}

void OS_Chimera::set_main_loop(MainLoop *p_main_loop) {
	main_loop = p_main_loop;
}

void OS_Chimera::delete_main_loop() {
	if (main_loop) {
		memdelete(main_loop);
	}
	main_loop = nullptr;
}

void OS_Chimera::finalize() {
	delete_main_loop();
}

OS_Chimera::OS_Chimera() {
	DisplayServerChimera::register_chimera_driver();
}
