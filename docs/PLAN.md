# The Godot 4.6 core: plan and log

This core is [chimera-core-godot47](https://github.com/ToolAssisted-run/chimera-core-godot47)
with Godot 4.6.3 pinned in place of 4.7.2. Every decision about how a Godot
game becomes a Chimera machine is in that repository's docs/PLAN.md, and the
code (`module/chimera`, `waterbox/`, `tests/`) is the same files; a change
to one is made to every Godot core.

## Why a core per version (user-decided, 2026-10-20)

A game exported with Godot 4.3 or later carries its scripts compiled to a
token format that only the same version loads, and a pack made by a newer
Godot is refused by an older one. The owner chose cores for 4.4, 4.5 and 4.6
beside the 4.7 one, each its own package, repository and roster entry (one
Chimera package holds one engine). The 4.4 core also takes games of 4.0 to
4.3, for which there is no core.

## What differs from the Godot 4.7 core

- `module/chimera/chimera_compat.h`: before 4.7 the display server's enums
  are DisplayServer's own (4.7 moved them to DisplayServerEnums).
- The test games (`tests/*.pck`) are exported by the Godot 4.6.3 editor
  (`tests/make-test-game.sh`); a 4.7 pack does not load here.
- The pack's header says which Godot exported it, and a game of another
  version is refused with the name of the core to use (in every Godot core).

## Measured (2026-10-20)

- `waterbox/run-gate.sh`: 40 passed, 0 failed, on Dodge the Creeps and the
  probe exported by the Godot 4.6.3 editor: native = sandbox on every
  picture, the sound and the probe's 154 lines; a savestate before every
  frame (36,999,502 bytes) changes nothing.
- `tests/run-frontend.sh`: 5 ok (chimera-run with a movie file and a
  project, Chimera's window, the default bindings).

## Drawing Size (2026-10-28, chimera issue 380)

As the Godot 4.7 core's (its docs/PLAN.md): the Drawing Size setting (100%, 75%,
50% or 25%) gives the game a window of that share of the size it asks for,
and the game draws at that size, a quarter of the pixels at 50%. Godot draws
every pixel on the CPU here, so a game that stretches its picture to its
window runs about twice as fast or more; one with a fixed size of its own is
not made faster. It is part of the machine: a movie needs the same value.
The test script checks that at 50% Dodge the Creeps draws 240x360 and plays
the same steps, native and in the sandbox.

## Drawing Code: AVX2 where the computer has it (user-decided, 2026-10-28, chimera issue 380)

As the Godot 4.7 core's (its docs/PLAN.md): the Drawing Code setting, AVX2 by
default, lets llvmpipe make x86-64-v3 code (256-bit vectors) on a computer
that has all of it (mesa-patches/0002), the same on every such computer;
Baseline, and a computer without it, keep the baseline of patch 0001. The
game plays the same either way. The test script checks the 256-bit code in
the sandbox and natively, native = sandbox with it, and that Baseline still
makes 128-bit code and plays the same steps.


## A game that asks for a file of the player's own (user-decided, 2026-11-14, chimera issue 567)

Super Mario Bros. Remastered 1.1.0 (GPL-3.0, Godot 4.6) did not start in
the Godot 4.6 core, and it needs the player's own Super Mario Bros. NES ROM.
The owner asked for support only through changes every Godot game could use
(2026-11-14). Two were needed.

- **The files slot takes a file of any kind** (user-decided, 2026-11-14).
  The game looks in the folder of its executable for any file ending in
  `.nes`, checks its hash and copies it to `user://baserom.nes`. The core
  already puts the files of the project's "Other files beside the game"
  slot in that folder (`/game`), but the slot listed eight extensions, and
  Chimera refused a project with a `.nes` there ("file '...' is not a format
  slot 'files' accepts"). The slot now declares no formats
  (`"formats": []`): Chimera takes any file there (a rule it documents and
  tests), the file picker shows every file, and the core is unchanged.
- **user://logs is there at power-on** (user-decided, 2026-11-14: "No side
  effects in Chimera are allowable. Any file system modifications need to be
  fully emulated."). The game's mod loader opens `user://logs/modloader.log`
  for writing without checking the result. On a desktop, Godot's own log
  file has made that folder; here Godot's log goes nowhere (see "Save
  data"), the open failed, and the export template, which leaves out
  GDScript's null checks, crashed the core at start ("the core crashed: it
  read or ran address (nil)", in `MethodBindTR<bool, String const&>`). The
  core's disk now holds an empty `/user/logs` from power-on. That disk is
  the core's memory, so the folder is in every state and nothing is written
  to the computer's files. An empty folder is no save data.

Tests: `waterbox/run-gate.sh` checks that the probe's own log opens in
user://logs, native and in the sandbox, with a save and load before every
frame, and from states at frame 0 and frame 25 (the probe removes the folder
at frame 10, so the later state is seen to carry the disk as the game left
it, where a new machine has the folder); and that Dodge the Creeps, which
writes nothing, exports no save data. The probe was exported again with this
core's Godot editor for these checks. `tests/run-frontend.sh` makes a project
with a generated `stand-in.nes` in the files slot and plays it to the
reference's picture.

Measured on jaffanator01 (2026-11-14):

- `waterbox/run-gate.sh`: 51 passed, 0 failed. Built without the folder,
  the three new user://logs checks failed (the probe printed
  `logs=[dir=false open=false]`) and the other 48 passed.
- `tests/run-frontend.sh`: 6 ok. Before the files slot changed, the new
  check failed with Chimera's refusal of `stand-in.nes`.
- Nothing reaches the computer's files: strace of `run-wbx` running the
  probe showed only reads of its inputs and libraries, and no folder made;
  no call in either build named `logs`.
- Super Mario Bros. Remastered 1.1.0 with the player's ROM in the files
  slot and nothing else, through a project: it verified the ROM, made its
  157 pictures from it, reached its title by frame 1000, and with inputs
  went through its menus into World 1-1, where Mario walked, jumped and
  lost his lives (4000 frames in chimera-run; 2400 frames in Chimera's
  window, run headless, to the same place). Before these changes it
  crashed at start. Its two GDExtensions (a GIF loader and Discord) do not
  load and it goes on without them; its HTTPS version check fails ("HTTPS
  is not available in this build") and it goes on. A whole level was not
  finished.

## Machine specification 3 (2026-11-20, owner's audit of the cores that carry llvmpipe)

The core now declares miniBox machine specification 3 (`__wbx_machine_spec` in
`module/chimera/chimera_core.cpp`). Why: this core carries Mesa's llvmpipe,
which makes code at run time with LLVM. Under specification 2 an `mmap` with a
bare address hint maps over whatever already lives at that address, and LLVM's
code generator asks for the page next to the one it just got. The Supermodel
core lost the first page of its picture buffer that way (its docs/PLAN.md,
2026-11-20). Under specification 3 a hint is used only where those pages are
free, and the call is placed elsewhere otherwise. The Godot 4.4 core's
docs/PLAN.md has the measurement of this on a game (9 of 35 hinted calls named
pages already mapped; specification 2 mapped on top of them, 3 did not).

What else specification 3 changes, and why it does not matter here: the machine
has its own clock (every clock read advances it one microsecond) and timed
waits follow it. Godot's time in this core is the core's own (`OS_Chimera`,
frames over the frame rate), so no game sees the machine's clock.

Tests on jaffanator01: `waterbox/run-gate.sh` 51 passed, 0 failed, 0 skipped; `tests/run-frontend.sh` 6 ok, 0 failed, 0 skipped;
Chimera's `test-packages` on the package 26 passed, 0 failed.

Old movies: on the Godot 4.4 core the same change was checked (2026-11-21): a movie recorded with the spec-2 package gives the same pictures on the spec-3 one. It was not repeated on this version, which has the same core code (OS_Chimera keeps Godot's time) and so is expected to behave the same.
