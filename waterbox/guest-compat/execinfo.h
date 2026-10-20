/* execinfo.h - for the guest build only. Godot's Linux crash handler
 * includes it, and the guest's C library (musl) has none. The handler is
 * never installed in the core (the core's OS does not install it), so the
 * calls only need to exist. */
#ifndef CHIMERA_EXECINFO_H
#define CHIMERA_EXECINFO_H
#ifdef __cplusplus
extern "C" {
#endif
static inline int backtrace(void **buffer, int size) { (void)buffer; (void)size; return 0; }
static inline char **backtrace_symbols(void *const *buffer, int size) { (void)buffer; (void)size; return 0; }
static inline void backtrace_symbols_fd(void *const *buffer, int size, int fd) { (void)buffer; (void)size; (void)fd; }
#ifdef __cplusplus
}
#endif
#endif
