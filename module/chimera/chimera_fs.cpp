/**************************************************************************/
/*  chimera_fs.cpp                                                        */
/**************************************************************************/
/* See chimera_fs.h.                                                      */
/**************************************************************************/

#include "chimera_fs.h"

#include "core/os/os.h"
#include "core/templates/sort_array.h"

#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

HashMap<String, ChimeraFSRef> *ChimeraFS::nodes = nullptr;

static uint64_t _now() {
	OS *os = OS::get_singleton();
	return os ? (uint64_t)os->get_unix_time() : 0;
}

void ChimeraFS::initialize() {
	if (nodes) {
		return;
	}
	nodes = memnew((HashMap<String, ChimeraFSRef>));
	make_dir("/");
	make_dir("/game");
	make_dir("/user");
	make_dir("/tmp");
	make_dir("/dev");
	make_file("/dev/null")->discard = true;
}

// An absolute path with no ".", "..", empty or trailing parts. A relative
// path is taken from /game, which is the working directory Godot is told.
String ChimeraFS::normalize(const String &p_path) {
	String p = p_path.replace("\\", "/");
	if (!p.begins_with("/")) {
		p = "/game/" + p;
	}
	Vector<String> parts = p.split("/", false);
	Vector<String> out;
	for (const String &part : parts) {
		if (part == ".") {
			continue;
		}
		if (part == "..") {
			if (!out.is_empty()) {
				out.remove_at(out.size() - 1);
			}
			continue;
		}
		out.push_back(part);
	}
	return "/" + String("/").join(out);
}

ChimeraFSRef ChimeraFS::find(const String &p_abs) {
	if (!nodes) {
		return ChimeraFSRef();
	}
	const ChimeraFSRef *n = nodes->getptr(p_abs);
	return n ? *n : ChimeraFSRef();
}

bool ChimeraFS::parent_exists(const String &p_abs) {
	if (p_abs == "/") {
		return true;
	}
	String parent = p_abs.get_base_dir();
	if (parent.is_empty()) {
		parent = "/";
	}
	ChimeraFSRef n = find(parent);
	return n.is_valid() && n->dir;
}

ChimeraFSRef ChimeraFS::make_file(const String &p_abs) {
	ChimeraFSRef n = find(p_abs);
	if (n.is_valid()) {
		return n->dir ? ChimeraFSRef() : n;
	}
	if (!parent_exists(p_abs)) {
		return ChimeraFSRef();
	}
	n.instantiate();
	n->modified_time = _now();
	nodes->insert(p_abs, n);
	return n;
}

Error ChimeraFS::make_dir(const String &p_abs) {
	ChimeraFSRef n = find(p_abs);
	if (n.is_valid()) {
		return n->dir ? ERR_ALREADY_EXISTS : ERR_FILE_CANT_WRITE;
	}
	if (!parent_exists(p_abs)) {
		return ERR_FILE_BAD_PATH;
	}
	n.instantiate();
	n->dir = true;
	n->modified_time = _now();
	nodes->insert(p_abs, n);
	return OK;
}

static bool _is_under(const String &p_path, const String &p_dir) {
	if (p_dir == "/") {
		return p_path != "/";
	}
	return p_path.length() > p_dir.length() && p_path.begins_with(p_dir) && p_path[p_dir.length()] == '/';
}

Error ChimeraFS::remove(const String &p_abs) {
	ChimeraFSRef n = find(p_abs);
	if (n.is_null() || p_abs == "/") {
		return ERR_FILE_NOT_FOUND;
	}
	if (n->is_mounted() || n->discard) {
		return ERR_FILE_CANT_WRITE;
	}
	if (n->dir) {
		for (const KeyValue<String, ChimeraFSRef> &E : *nodes) {
			if (_is_under(E.key, p_abs)) {
				return ERR_BUSY; // not empty, as rmdir says
			}
		}
	}
	nodes->erase(p_abs);
	return OK;
}

Error ChimeraFS::rename(const String &p_from, const String &p_to) {
	ChimeraFSRef n = find(p_from);
	if (n.is_null() || p_from == "/") {
		return ERR_FILE_NOT_FOUND;
	}
	if (n->is_mounted() || n->discard) {
		return ERR_FILE_CANT_WRITE;
	}
	if (!parent_exists(p_to) || _is_under(p_to, p_from)) {
		return ERR_FILE_BAD_PATH;
	}
	ChimeraFSRef existing = find(p_to);
	if (existing.is_valid()) {
		if (existing->dir || existing->is_mounted() || n->dir) {
			return ERR_ALREADY_EXISTS;
		}
		nodes->erase(p_to);
	}
	// a directory takes everything under it along
	Vector<String> moved;
	if (n->dir) {
		for (const KeyValue<String, ChimeraFSRef> &E : *nodes) {
			if (_is_under(E.key, p_from)) {
				moved.push_back(E.key);
			}
		}
	}
	nodes->erase(p_from);
	nodes->insert(p_to, n);
	for (const String &old : moved) {
		ChimeraFSRef child = find(old);
		nodes->erase(old);
		nodes->insert(p_to + old.substr(p_from.length()), child);
	}
	return OK;
}

// The names in a directory, sorted: a directory listing must not depend on
// the order the files were made in.
void ChimeraFS::list(const String &p_dir_abs, Vector<String> &r_names) {
	r_names.clear();
	if (!nodes) {
		return;
	}
	for (const KeyValue<String, ChimeraFSRef> &E : *nodes) {
		if (!_is_under(E.key, p_dir_abs)) {
			continue;
		}
		String rest = E.key.substr(p_dir_abs == "/" ? 1 : p_dir_abs.length() + 1);
		if (rest.contains_char('/')) {
			continue;
		}
		r_names.push_back(rest);
	}
	r_names.sort();
}

bool ChimeraFS::mount(const String &p_abs, const char *p_host_name) {
	int fd = ::open(p_host_name, O_RDONLY);
	if (fd < 0) {
		return false;
	}
	off_t size = ::lseek(fd, 0, SEEK_END);
	::lseek(fd, 0, SEEK_SET);
	if (size < 0) {
		::close(fd);
		return false;
	}
	ChimeraFSRef n = make_file(p_abs);
	if (n.is_null()) {
		::close(fd);
		return false;
	}
	n->mount_fd = fd;
	n->mount_size = (uint64_t)size;
	n->data.clear();
	return true;
}

void ChimeraFS::put(const String &p_abs, const uint8_t *p_data, uint64_t p_size) {
	// make the folders a save file sits in, as the game had made them
	String dir = p_abs.get_base_dir();
	Vector<String> parts = dir.split("/", false);
	String cur;
	for (const String &part : parts) {
		cur += "/" + part;
		make_dir(cur);
	}
	ChimeraFSRef n = make_file(p_abs);
	if (n.is_null() || n->is_mounted()) {
		return;
	}
	n->data.resize(p_size);
	if (p_size) {
		memcpy(n->data.ptrw(), p_data, p_size);
	}
}

void ChimeraFS::list_user_files(Vector<String> &r_paths) {
	r_paths.clear();
	if (!nodes) {
		return;
	}
	for (const KeyValue<String, ChimeraFSRef> &E : *nodes) {
		if (_is_under(E.key, "/user") && !E.value->dir) {
			r_paths.push_back(E.key.substr(6));
		}
	}
	r_paths.sort();
}

/* ---- files ---------------------------------------------------------- */

Error FileAccessChimera::open_internal(const String &p_path, int p_mode_flags) {
	close();
	String abs = ChimeraFS::normalize(fix_path(p_path));
	ChimeraFSRef n = ChimeraFS::find(abs);
	if (n.is_valid() && n->dir) {
		last_error = ERR_FILE_CANT_OPEN;
		return last_error;
	}
	if (p_mode_flags == READ) {
		if (n.is_null()) {
			last_error = ERR_FILE_NOT_FOUND;
			return last_error;
		}
	} else {
		if (n.is_valid() && n->is_mounted()) {
			last_error = ERR_FILE_CANT_WRITE;
			return last_error;
		}
		if (n.is_null()) {
			if (p_mode_flags == READ_WRITE) {
				last_error = ERR_FILE_NOT_FOUND;
				return last_error;
			}
			n = ChimeraFS::make_file(abs);
			if (n.is_null()) {
				last_error = ERR_FILE_CANT_OPEN;
				return last_error;
			}
		}
		if ((p_mode_flags == WRITE || p_mode_flags == WRITE_READ) && !n->discard) {
			n->data.clear();
			n->modified_time = _now();
		}
		writable = true;
	}
	node = n;
	path = abs;
	pos = 0;
	eof = false;
	cache_len = 0;
	last_error = OK;
	return OK;
}

uint64_t FileAccessChimera::get_length() const {
	return node.is_valid() ? node->size() : 0;
}

void FileAccessChimera::seek(uint64_t p_position) {
	ERR_FAIL_COND_MSG(node.is_null(), "File must be opened before use.");
	pos = p_position;
	eof = false;
}

void FileAccessChimera::seek_end(int64_t p_position) {
	ERR_FAIL_COND_MSG(node.is_null(), "File must be opened before use.");
	int64_t p = (int64_t)node->size() + p_position;
	pos = p < 0 ? 0 : (uint64_t)p;
	eof = false;
}

uint64_t FileAccessChimera::_read_mounted(uint8_t *p_dst, uint64_t p_length) const {
	const uint64_t window = 65536;
	uint64_t done = 0;
	while (done < p_length) {
		uint64_t at = pos + done;
		if (at >= node->mount_size) {
			break;
		}
		if (at >= cache_start && at < cache_start + cache_len) {
			uint64_t n = MIN(p_length - done, cache_start + cache_len - at);
			memcpy(p_dst + done, cache.ptr() + (at - cache_start), n);
			done += n;
			continue;
		}
		// a large read goes straight to the destination
		if (p_length - done >= window) {
			ssize_t got = ::pread(node->mount_fd, p_dst + done, p_length - done, (off_t)at);
			if (got <= 0) {
				break;
			}
			done += (uint64_t)got;
			continue;
		}
		if (cache.size() < (int64_t)window) {
			cache.resize(window);
		}
		ssize_t got = ::pread(node->mount_fd, cache.ptrw(), window, (off_t)at);
		if (got <= 0) {
			cache_len = 0;
			break;
		}
		cache_start = at;
		cache_len = (uint64_t)got;
	}
	return done;
}

uint64_t FileAccessChimera::get_buffer(uint8_t *p_dst, uint64_t p_length) const {
	ERR_FAIL_COND_V_MSG(node.is_null(), -1, "File must be opened before use.");
	if (p_length == 0) {
		return 0;
	}
	uint64_t size = node->size();
	uint64_t avail = pos < size ? size - pos : 0;
	uint64_t want = MIN(p_length, avail);
	uint64_t got;
	if (node->is_mounted()) {
		got = _read_mounted(p_dst, want);
	} else {
		if (want) {
			memcpy(p_dst, node->data.ptr() + pos, want);
		}
		got = want;
	}
	pos += got;
	if (got < p_length) {
		eof = true;
	}
	return got;
}

Error FileAccessChimera::resize(int64_t p_length) {
	ERR_FAIL_COND_V_MSG(node.is_null(), FAILED, "File must be opened before use.");
	ERR_FAIL_COND_V(!writable || p_length < 0, ERR_FILE_CANT_WRITE);
	node->data.resize(p_length);
	node->modified_time = _now();
	return OK;
}

bool FileAccessChimera::store_buffer(const uint8_t *p_src, uint64_t p_length) {
	ERR_FAIL_COND_V_MSG(node.is_null(), false, "File must be opened before use.");
	ERR_FAIL_COND_V(!writable, false);
	if (p_length == 0 || node->discard) {
		return true;
	}
	ERR_FAIL_NULL_V(p_src, false);
	uint64_t end = pos + p_length;
	if (end > (uint64_t)node->data.size()) {
		node->data.resize(end);
	}
	memcpy(node->data.ptrw() + pos, p_src, p_length);
	pos = end;
	node->modified_time = _now();
	return true;
}

bool FileAccessChimera::file_exists(const String &p_name) {
	ChimeraFSRef n = ChimeraFS::find(ChimeraFS::normalize(fix_path(p_name)));
	return n.is_valid() && !n->dir;
}

uint64_t FileAccessChimera::_get_modified_time(const String &p_file) {
	ChimeraFSRef n = ChimeraFS::find(ChimeraFS::normalize(fix_path(p_file)));
	return n.is_valid() ? n->modified_time : 0;
}

#if CHIMERA_GODOT_MINOR >= 5
int64_t FileAccessChimera::_get_size(const String &p_file) {
	ChimeraFSRef n = ChimeraFS::find(ChimeraFS::normalize(fix_path(p_file)));
	return n.is_valid() && !n->dir ? (int64_t)n->size() : -1;
}
#endif

bool FileAccessChimera::_get_read_only_attribute(const String &p_file) {
	ChimeraFSRef n = ChimeraFS::find(ChimeraFS::normalize(fix_path(p_file)));
	return n.is_valid() && n->is_mounted();
}

void FileAccessChimera::close() {
	node.unref();
	path = String();
	writable = false;
	pos = 0;
	eof = false;
	cache.clear();
	cache_len = 0;
}

FileAccessChimera::~FileAccessChimera() {
	close();
}

/* ---- folders -------------------------------------------------------- */

String DirAccessChimera::_abs(const String &p_path) const {
	String p = fix_path(p_path);
	if (p.is_relative_path()) {
		p = current_dir.path_join(p);
	}
	return ChimeraFS::normalize(p);
}

Error DirAccessChimera::list_dir_begin() {
	list_dir_end();
	ChimeraFSRef n = ChimeraFS::find(current_dir);
	if (n.is_null() || !n->dir) {
		return ERR_CANT_OPEN;
	}
	listing.push_back(".");
	listing.push_back("..");
	Vector<String> names;
	ChimeraFS::list(current_dir, names);
	listing.append_array(names);
	listing_pos = 0;
	return OK;
}

String DirAccessChimera::get_next() {
	if (listing_pos < 0 || listing_pos >= listing.size()) {
		list_dir_end();
		return String();
	}
	String name = listing[listing_pos++];
	if (name == "." || name == "..") {
		cur_is_dir = true;
	} else {
		ChimeraFSRef n = ChimeraFS::find(current_dir == "/" ? "/" + name : current_dir + "/" + name);
		cur_is_dir = n.is_valid() && n->dir;
	}
	cur_is_hidden = name.begins_with(".") && name != "." && name != "..";
	return name;
}

void DirAccessChimera::list_dir_end() {
	listing.clear();
	listing_pos = -1;
	cur_is_dir = false;
	cur_is_hidden = false;
}

Error DirAccessChimera::change_dir(String p_dir) {
	String abs = _abs(p_dir);
	ChimeraFSRef n = ChimeraFS::find(abs);
	if (n.is_null() || !n->dir) {
		return ERR_INVALID_PARAMETER;
	}
	String base = _get_root_path();
	if (!base.is_empty() && !abs.begins_with(ChimeraFS::normalize(base))) {
		return OK; // as on a disk: user:// cannot be left by cd ..
	}
	current_dir = abs;
	return OK;
}

String DirAccessChimera::get_current_dir(bool p_include_drive) const {
	String base = _get_root_path();
	if (!base.is_empty()) {
		String nbase = ChimeraFS::normalize(base);
		String bd = current_dir.begins_with(nbase) ? current_dir.substr(nbase.length()) : current_dir;
		if (bd.begins_with("/")) {
			return _get_root_string() + bd.substr(1);
		}
		return _get_root_string() + bd;
	}
	return current_dir;
}

Error DirAccessChimera::make_dir(String p_dir) {
	return ChimeraFS::make_dir(_abs(p_dir));
}

bool DirAccessChimera::file_exists(String p_file) {
	ChimeraFSRef n = ChimeraFS::find(_abs(p_file));
	return n.is_valid() && !n->dir;
}

bool DirAccessChimera::dir_exists(String p_dir) {
	ChimeraFSRef n = ChimeraFS::find(_abs(p_dir));
	return n.is_valid() && n->dir;
}

Error DirAccessChimera::rename(String p_from, String p_to) {
	return ChimeraFS::rename(_abs(p_from), _abs(p_to));
}

Error DirAccessChimera::remove(String p_name) {
	return ChimeraFS::remove(_abs(p_name));
}

DirAccessChimera::DirAccessChimera() {
	current_dir = "/game";
}
