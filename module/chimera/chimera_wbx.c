/* chimera_wbx.c - the project's settings and files, through the guest kit's
 * C headers (waterbox_settings.h and waterbox_slots.h read the "settings"
 * and "slots" files Chimera mounts), for the C++ of chimera_core.cpp. */
#include <waterbox_settings.h>
#include <waterbox_slots.h>

int chimera_setting_str(const char *key, char *out, int outsz)
{
	return wbx_setting_str(key, out, outsz);
}

long chimera_setting_long(const char *key, long dflt)
{
	return wbx_setting_long(key, dflt);
}

int chimera_slot_count(const char *id)
{
	return wbx_slot_count(id);
}

const char *chimera_slot_name(const char *id, int index, char *out, int outsz)
{
	return wbx_slot_name(id, index, out, outsz);
}
