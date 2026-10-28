# AGENTS.md - chimera-core-godot46

The Godot 4.6.3 core: chimera-core-godot47 with Godot 4.6.3 pinned. Keep its
`module/`, `waterbox/` and `tests/` the same files as the other Godot cores.

The Godot Engine (4.6.3) as a Chimera core: Godot's export template built
for miniBox's sandbox with a Chimera module in it. The user brings a game's
`.pck`. Read Chimera's `docs/new-core.md` and this repository's
`docs/PLAN.md` first.

## Layout

- `extern/godot` - Godot at 4.6.3-stable, a submodule. Never built in and
  never edited: `build-godot.sh` copies it to `build/godot-<flavour>` and
  applies `patches/` there.
- `module/chimera` - the Chimera module (given to SCons as a custom module):
  `chimera_os` (time, date, environment), `chimera_fs` (the disk in memory),
  `chimera_display` (OSMesa window, readback), `chimera_core.cpp` (the
  core's exports, input, sound), `chimera_wbx.c` (settings and slots).
- `patches/` - numbered patches to Godot.
- `waterbox/` - the scripts and the package's declarations
  (`waterbox.config`, `file_slots.json`, `default_keybinds.json`,
  `package-licenses.json`), the harness (`gate-harness.h`, `run-native.c`,
  `run-wbx.c`, `input-names.h`), `run-gate.sh`.
- `tests/` - the test games (Dodge the Creeps, MIT; the probe, this
  repository's), `run-frontend.sh` and its helpers.
- `build/` - every output. Ignored by git.

## Commands, in order

    mb=<chimera>/extern/chimera-common-minibox
    waterbox/build-llvm.sh native && waterbox/build-llvm.sh guest -m $mb
    waterbox/setup-mesa.sh -m $mb && waterbox/setup-mesa.sh -n
    waterbox/make-native-libm.sh $mb
    waterbox/build-godot.sh native -m $mb && waterbox/build-godot.sh guest -m $mb
    waterbox/link-core.sh native -m $mb && waterbox/link-core.sh guest -m $mb
    waterbox/run-gate.sh -m $mb                      # the core's tests
    waterbox/build-package.sh -r <chimera>           # the package, installed
    tests/run-frontend.sh --chimera-root <chimera>   # through Chimera

## Rules

- Everything the game can observe comes from the core: no host clock, no
  host file, no environment. A new question Godot asks its OS gets an answer
  in `chimera_os` that is the same in both builds.
- The native reference links the sandbox's math (`make-native-libm.sh`);
  never let it fall back to glibc's.
- llvmpipe sees one CPU everywhere (mesa-patches/0001), but for the one
  question patch 0002 asks when Drawing Code is AVX2 (owner, 2026-10-28):
  does this computer have all of x86-64-v3. Nothing else may give Mesa or
  LLVM the host's CPU features, and nothing its core count.
- A change to `waterbox.config`'s buttons changes `chimera_core.cpp`'s table
  and `input-names.h` the same way, in the same order.
- Every new test is broken on purpose once and seen to fail.
- Write in plain language; prose is ASCII. Commits: `type(scope): what
  changed`, body with how it was tested, Chimera's trailers.
- Stop a build by its PID, never with `pkill -f`.
