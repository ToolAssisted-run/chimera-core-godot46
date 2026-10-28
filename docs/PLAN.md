# The Godot 4.6 core: plan and log

This core is [chimera-core-godot](https://github.com/ToolAssisted-run/chimera-core-godot)
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

## What differs from the 4.7 core

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

As the 4.7 core's (its docs/PLAN.md): the Drawing Size setting (100%, 75%,
50% or 25%) gives the game a window of that share of the size it asks for,
and the game draws at that size, a quarter of the pixels at 50%. Godot draws
every pixel on the CPU here, so a game that stretches its picture to its
window runs about twice as fast or more; one with a fixed size of its own is
not made faster. It is part of the machine: a movie needs the same value.
The test script checks that at 50% Dodge the Creeps draws 240x360 and plays
the same steps, native and in the sandbox.
