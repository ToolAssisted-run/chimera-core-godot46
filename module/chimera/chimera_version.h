/**************************************************************************/
/*  chimera_version.h                                                     */
/**************************************************************************/
/* The minor version of the Godot 4 this module is built against, for     */
/* what differs between versions (chimera_compat.h, chimera_os.h).        */
/**************************************************************************/

#pragma once

#include "core/version.h"

#ifdef GODOT_VERSION_MINOR
#define CHIMERA_GODOT_MINOR GODOT_VERSION_MINOR
#else
#define CHIMERA_GODOT_MINOR VERSION_MINOR
#endif
