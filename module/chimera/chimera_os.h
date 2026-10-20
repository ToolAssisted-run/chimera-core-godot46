/**************************************************************************/
/*  chimera_os.h                                                          */
/**************************************************************************/
/* The OS Godot runs on inside the Chimera core.                          */
/*                                                                        */
/* Godot asks its OS for the time, the date, the files, the environment,  */
/* the number of processors and a source of random bytes. On a desktop    */
/* those come from the computer; here every answer comes from the core,   */
/* so that a game run twice from the same inputs does the same thing, and */
/* the native reference build and the sandbox build answer alike:         */
/*                                                                        */
/*  - time is counted in frames: Godot's clock reads frame / fps seconds  */
/*    and stands still within a frame;                                    */
/*  - the calendar starts at the project's Start Time setting and runs    */
/*    with the frames, in UTC;                                            */
/*  - files are the core's own (chimera_fs.h): the game's pack, read only,*/
/*    and an in-memory disk for everything the game writes;               */
/*  - there are no processes, no environment variables, no network;       */
/*  - "random" bytes come from a generator seeded by the core.            */
/**************************************************************************/

#pragma once

#include "chimera_version.h"

#include "drivers/unix/os_unix.h"

class MainLoop;

class OS_Chimera : public OS_Unix {
	MainLoop *main_loop = nullptr;

	// The clock: frames shown so far, at fps frames a second.
	uint64_t frame_count = 0;
	uint32_t fps = 60;
	// Seconds since 1970 when the machine was switched on (a setting).
	int64_t start_unix_time = 0;
	// Where the game's executable would be: Godot looks for its pack beside
	// it, or inside it (chimera_core.cpp).
	String executable_path = "/game/godot";
	// splitmix64 state for get_entropy().
	mutable uint64_t entropy_state = 0x4368696d65726121ull;

protected:
	virtual void initialize() override;
	virtual void initialize_joypads() override {}

	virtual void set_main_loop(MainLoop *p_main_loop) override;
	virtual void delete_main_loop() override;

	virtual void finalize() override;

	virtual bool _check_internal_feature_support(const String &p_feature) override;

public:
	static OS_Chimera *get_chimera_singleton() { return static_cast<OS_Chimera *>(get_singleton()); }

	void set_clock(uint32_t p_fps, int64_t p_start_unix_time);
	void set_executable_path(const String &p_path) { executable_path = p_path; }
	void advance_frame() { frame_count++; }
	uint64_t get_frame_count() const { return frame_count; }
	uint32_t get_fps() const { return fps; }

	virtual String get_name() const override { return "Linux"; }
	virtual String get_identifier() const override { return "linuxbsd"; }
	virtual String get_distribution_name() const override { return "Chimera"; }
	virtual String get_version() const override { return ""; }
	virtual String get_processor_name() const override { return "Chimera"; }
	virtual Vector<String> get_video_adapter_driver_info() const override { return Vector<String>(); }

	virtual MainLoop *get_main_loop() const override { return main_loop; }

	virtual uint64_t get_ticks_usec() const override;
	virtual double get_unix_time() const override;
	virtual DateTime get_datetime(bool p_utc) const override;
	virtual TimeZoneInfo get_time_zone_info() const override;
	virtual void delay_usec(uint32_t p_usec) const override {}

	virtual Error get_entropy(uint8_t *r_buffer, int p_bytes) override;

	virtual int get_processor_count() const override { return 1; }
	virtual String get_unique_id() const override { return "chimera"; }
	virtual Dictionary get_memory_info() const override;

	virtual String get_executable_path() const override;
	virtual uint64_t get_embedded_pck_offset() const override { return 0; }
	virtual Error set_cwd(const String &p_cwd) override { return ERR_UNAVAILABLE; }
#if CHIMERA_GODOT_MINOR >= 6
	virtual String get_cwd() const override { return "/game"; }
#endif

	virtual String get_user_data_dir(const String &p_user_dir) const override { return "/user"; }
	virtual String get_data_path() const override { return "/user"; }
	virtual String get_config_path() const override { return "/user"; }
	virtual String get_cache_path() const override { return "/user"; }
	virtual String get_temp_path() const override { return "/tmp"; }
	virtual String get_system_dir(SystemDir p_dir, bool p_shared_storage = true) const override { return String(); }

	virtual String get_locale() const override { return "en_US"; }
	virtual String get_stdin_string(int64_t p_buffer_size = 1024) override { return String(); }
	virtual PackedByteArray get_stdin_buffer(int64_t p_buffer_size = 1024) override { return PackedByteArray(); }

	virtual bool has_environment(const String &p_var) const override { return false; }
	virtual String get_environment(const String &p_var) const override { return String(); }
	virtual void set_environment(const String &p_var, const String &p_value) const override {}
	virtual void unset_environment(const String &p_var) const override {}

	virtual Error execute(const String &p_path, const List<String> &p_arguments, String *r_pipe = nullptr, int *r_exitcode = nullptr, bool read_stderr = false, Mutex *p_pipe_mutex = nullptr, bool p_open_console = false) override { return ERR_UNAVAILABLE; }
	virtual Dictionary execute_with_pipe(const String &p_path, const List<String> &p_arguments, bool p_blocking = true) override { return Dictionary(); }
	virtual Error create_process(const String &p_path, const List<String> &p_arguments, ProcessID *r_child_id = nullptr, bool p_open_console = false) override { return ERR_UNAVAILABLE; }
	virtual Error kill(const ProcessID &p_pid) override { return ERR_UNAVAILABLE; }
	virtual int get_process_id() const override { return 1; }
	virtual bool is_process_running(const ProcessID &p_pid) const override { return false; }
	virtual int get_process_exit_code(const ProcessID &p_pid) const override { return -1; }
	virtual Error shell_open(const String &p_uri) override { return ERR_UNAVAILABLE; }

	virtual Error open_dynamic_library(const String &p_path, void *&p_library_handle, GDExtensionData *p_data = nullptr) override { return ERR_UNAVAILABLE; }

#if CHIMERA_GODOT_MINOR >= 5
	virtual String multibyte_to_string(const String &p_encoding, const PackedByteArray &p_array) const override { return String(); }
	virtual PackedByteArray string_to_multibyte(const String &p_encoding, const String &p_string) const override { return PackedByteArray(); }
#endif

	OS_Chimera();
};
