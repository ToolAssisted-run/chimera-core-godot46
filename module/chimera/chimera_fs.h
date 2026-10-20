/**************************************************************************/
/*  chimera_fs.h                                                          */
/**************************************************************************/
/* The disk Godot sees inside the Chimera core.                           */
/*                                                                        */
/* One tree serves res://, user:// and plain paths alike, and all of it   */
/* lives in the core's own memory, which a savestate carries:             */
/*                                                                        */
/*   /game   the files the project brings - the game's pack and any       */
/*           others - read only, read from the files Chimera mounted      */
/*           (the sandbox's descriptors survive a savestate; their bytes  */
/*           are never copied into memory);                               */
/*   /user   user://, where a game keeps its saves and settings: empty    */
/*           when the machine starts, unless the project brought save     */
/*           data, and written in memory;                                 */
/*   /tmp    and anything else a game creates.                            */
/*                                                                        */
/* Nothing here reaches the computer's own disk, so a game cannot find a  */
/* file one run and miss it the next, and loading a savestate puts back   */
/* every file as it was at that frame.                                    */
/**************************************************************************/

#pragma once

#include "core/io/dir_access.h"
#include "core/io/file_access.h"
#include "core/templates/hash_map.h"

#include "chimera_version.h"

#ifndef GDSOFTCLASS
// before 4.5 a class of this kind declares nothing
#define GDSOFTCLASS(m_class, m_inherits)
#endif

// A file or folder. Not a Godot Object: the core makes the tree before
// Godot's object database exists, while Godot is not yet set up.
struct ChimeraFSNode {
	int refs = 0;
	bool dir = false;
	// /dev/null: takes every write and keeps nothing
	bool discard = false;
	// A file the project brought: read from a descriptor Chimera mounted.
	int mount_fd = -1;
	uint64_t mount_size = 0;
	// A file in memory.
	Vector<uint8_t> data;
	uint64_t modified_time = 0;

	bool is_mounted() const { return mount_fd >= 0; }
	uint64_t size() const { return is_mounted() ? mount_size : (uint64_t)data.size(); }
};

// Shares a node between the tree and the files open on it, so that a file
// removed while open stays readable through that handle, as on a disk.
// Everything runs on one thread.
class ChimeraFSRef {
	ChimeraFSNode *n = nullptr;

	void _set(ChimeraFSNode *p_n) {
		if (p_n) {
			p_n->refs++;
		}
		unref();
		n = p_n;
	}

public:
	ChimeraFSNode *operator->() const { return n; }
	ChimeraFSNode *ptr() const { return n; }
	bool is_valid() const { return n != nullptr; }
	bool is_null() const { return n == nullptr; }
	void instantiate() { _set(memnew(ChimeraFSNode)); }
	void unref() {
		if (n && --n->refs == 0) {
			memdelete(n);
		}
		n = nullptr;
	}
	ChimeraFSRef &operator=(const ChimeraFSRef &p_other) {
		_set(p_other.n);
		return *this;
	}
	ChimeraFSRef() {}
	ChimeraFSRef(const ChimeraFSRef &p_other) { _set(p_other.n); }
	~ChimeraFSRef() { unref(); }
};

class ChimeraFS {
	static HashMap<String, ChimeraFSRef> *nodes;

public:
	static void initialize();
	static String normalize(const String &p_path);
	static ChimeraFSRef find(const String &p_abs);
	static bool parent_exists(const String &p_abs);
	static ChimeraFSRef make_file(const String &p_abs);
	static Error make_dir(const String &p_abs);
	static Error remove(const String &p_abs);
	static Error rename(const String &p_from, const String &p_to);
	static void list(const String &p_dir_abs, Vector<String> &r_names);

	// Init-time helpers for the core.
	static bool mount(const String &p_abs, const char *p_host_name);
	static void put(const String &p_abs, const uint8_t *p_data, uint64_t p_size);

	// Every file under /user, as paths relative to it, in a stable order.
	static void list_user_files(Vector<String> &r_paths);
};

class FileAccessChimera : public FileAccess {
	GDSOFTCLASS(FileAccessChimera, FileAccess);

	ChimeraFSRef node;
	String path;
	bool writable = false;
	mutable uint64_t pos = 0;
	mutable bool eof = false;
	mutable Error last_error = OK;

	// A small window of the mounted file, so that Godot's many 4-byte reads
	// of a pack's directory are not each a call into the sandbox.
	mutable Vector<uint8_t> cache;
	mutable uint64_t cache_start = 0;
	mutable uint64_t cache_len = 0;

	uint64_t _read_mounted(uint8_t *p_dst, uint64_t p_length) const;

public:
	virtual Error open_internal(const String &p_path, int p_mode_flags) override;
	virtual bool is_open() const override { return node.is_valid(); }

	virtual String get_path() const override { return path; }
	virtual String get_path_absolute() const override { return path; }

	virtual void seek(uint64_t p_position) override;
	virtual void seek_end(int64_t p_position = 0) override;
	virtual uint64_t get_position() const override { return pos; }
	virtual uint64_t get_length() const override;

	virtual bool eof_reached() const override { return eof; }

	virtual uint64_t get_buffer(uint8_t *p_dst, uint64_t p_length) const override;

	virtual Error get_error() const override { return last_error; }

	virtual Error resize(int64_t p_length) override;
	virtual void flush() override {}
	virtual bool store_buffer(const uint8_t *p_src, uint64_t p_length) override;

	virtual bool file_exists(const String &p_name) override;

	virtual uint64_t _get_modified_time(const String &p_file) override;
#if CHIMERA_GODOT_MINOR >= 5
	virtual uint64_t _get_access_time(const String &p_file) override { return _get_modified_time(p_file); }
	virtual int64_t _get_size(const String &p_file) override;
#endif

	virtual BitField<FileAccess::UnixPermissionFlags> _get_unix_permissions(const String &p_file) override { return 0644; }
	virtual Error _set_unix_permissions(const String &p_file, BitField<FileAccess::UnixPermissionFlags> p_permissions) override { return OK; }

	virtual bool _get_hidden_attribute(const String &p_file) override { return false; }
	virtual Error _set_hidden_attribute(const String &p_file, bool p_hidden) override { return ERR_UNAVAILABLE; }
	virtual bool _get_read_only_attribute(const String &p_file) override;
	virtual Error _set_read_only_attribute(const String &p_file, bool p_ro) override { return ERR_UNAVAILABLE; }

	virtual void close() override;

	virtual ~FileAccessChimera() override;
};

class DirAccessChimera : public DirAccess {
	GDSOFTCLASS(DirAccessChimera, DirAccess);

	String current_dir = "/";
	Vector<String> listing;
	int listing_pos = -1;
	bool cur_is_dir = false;
	bool cur_is_hidden = false;

	String _abs(const String &p_path) const;

public:
	virtual Error list_dir_begin() override;
	virtual String get_next() override;
	virtual bool current_is_dir() const override { return cur_is_dir; }
	virtual bool current_is_hidden() const override { return cur_is_hidden; }
	virtual void list_dir_end() override;

	virtual int get_drive_count() override { return 0; }
	virtual String get_drive(int p_drive) override { return String(); }

	virtual Error change_dir(String p_dir) override;
	virtual String get_current_dir(bool p_include_drive = true) const override;
	virtual Error make_dir(String p_dir) override;

	virtual bool file_exists(String p_file) override;
	virtual bool dir_exists(String p_dir) override;

	virtual uint64_t get_space_left() override { return 1ull << 30; }

	virtual Error rename(String p_from, String p_to) override;
	virtual Error remove(String p_name) override;

	virtual bool is_link(String p_file) override { return false; }
	virtual String read_link(String p_file) override { return p_file; }
	virtual Error create_link(String p_source, String p_target) override { return FAILED; }

	virtual String get_filesystem_type() const override { return "CHIMERA"; }

	DirAccessChimera();
};
