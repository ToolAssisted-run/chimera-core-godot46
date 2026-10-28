/**************************************************************************/
/*  chimera_compat.h                                                      */
/**************************************************************************/
/* The one module builds against every Godot 4 version a Chimera core     */
/* pins (chimera-core-godot47 and its siblings for older versions). What  */
/* differs between those versions is answered here, by version.           */
/**************************************************************************/

#pragma once

#include "chimera_version.h"

// 4.6 moved the display server's headers into servers/display/
#if __has_include("servers/display/display_server_headless.h")
#include "servers/display/display_server_headless.h"
#else
#include "servers/display_server_headless.h"
#endif

#ifndef GDSOFTCLASS
// before 4.5 a class of this kind declares nothing
#define GDSOFTCLASS(m_class, m_inherits)
#endif

#if CHIMERA_GODOT_MINOR < 7
// 4.7 moved the display server's enums and constants into DisplayServerEnums;
// before, they are DisplayServer's own.
using DisplayServerEnums = DisplayServer;
#endif

// The games this core runs: those exported with Godot 4.<minor> for every
// minor from CHIMERA_ACCEPTS_MINOR_FROM to the core's own. The oldest core
// (4.4) also takes the versions no core is built for (4.0 to 4.3); each of
// the others takes its own (chimera_core.cpp reads the pack's header).
#if CHIMERA_GODOT_MINOR <= 4
#define CHIMERA_ACCEPTS_MINOR_FROM 0
#else
#define CHIMERA_ACCEPTS_MINOR_FROM CHIMERA_GODOT_MINOR
#endif
// the versions there is a core for
#define CHIMERA_OLDEST_CORE_MINOR 4
#define CHIMERA_NEWEST_CORE_MINOR 7
