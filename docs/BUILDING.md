# Building and testing the Godot core

## Requirements

- Linux on x86-64, with GCC 14 (the guest kit's compiler is the system's,
  over miniBox's sysroot), Python 3, git, curl, `patch`, binutils.
- meson, ninja, cmake and Python's `mako` and `packaging` (LLVM's and
  Mesa's builds; `setup-mesa.sh` makes a private venv when they are missing).
- SCons: `build-godot.sh` installs a pinned SCons under `build/tools` with
  pip the first time.
- A Chimera checkout beside this one, with miniBox built: its host library
  (`meson-linux`) and its C++ guest toolchain (`meson-cpp`):

      mb=<chimera>/extern/chimera-common-minibox
      meson setup "$mb/build/meson-linux" "$mb" && meson compile -C "$mb/build/meson-linux"
      meson setup "$mb/build/meson-cpp" "$mb" -Dguest_cpp=true && meson compile -C "$mb/build/meson-cpp"

- For the frontend test: Chimera built (its `chimera-run` and its window,
  built by `<chimera>/tools/ci-chimera.sh`). The window runs headless and
  needs no display.

## Sources

    git clone --recursive https://github.com/ToolAssisted-run/chimera-core-godot4646
    git submodule update --init extern/godot

`extern/godot` is Godot at `4.6.3-stable`. It is never built in: every
build copies it to `build/godot-<flavour>` and applies `patches/` there.

## The steps

All from the repository's root, with `-m <chimera>/extern/chimera-common-minibox`:

1. `waterbox/build-llvm.sh native` and `waterbox/build-llvm.sh guest -m
   <mb>`: LLVM 18.1.8 for llvmpipe (downloads the source once into
   `build/deps`; most of an hour each on 16 threads; the guest needs the
   native one's table generator). Then `waterbox/setup-mesa.sh -n` and
   `waterbox/setup-mesa.sh -m <mb>`: Mesa 24.0.9's llvmpipe and OSMesa for
   this computer and the guest, with `waterbox/mesa-patches/` applied
   (about 5 minutes each).
2. `waterbox/make-native-libm.sh <mb>`: the sandbox's own math functions for
   the native build (`build/native-libm`).
3. `waterbox/build-godot.sh native -m <mb>` and `waterbox/build-godot.sh
   guest -m <mb>`: Godot with the Chimera module, as static libraries (a few
   minutes each on 16 threads; `SCONS_EXTRA=-k` lists every error at once).
4. `waterbox/link-core.sh native -m <mb>`: `build/native/run-native`, the
   native reference. `waterbox/link-core.sh guest -m <mb>`:
   `build/guest/core.wbx`, checked by miniBox's `check-wbx.sh`, and
   `build/guest/run-wbx`, which runs it in the sandbox.
5. `waterbox/build-package.sh -m <mb>` (or `-r <chimera>` to install it in
   that checkout's `build/Cores`): `godot46.chimeraCore`. It does steps 1, 3
   and 4 for the guest itself.

`waterbox/jit-symbols.c` is linked into both builds: it answers LLVM's
`dlopen(NULL)` (the sandbox's C library has no dynamic loader) and gives
no function by name but its short table, in both builds alike.

## The tests

- `waterbox/run-gate.sh -m <mb>`: builds everything, then the core's tests
  on Dodge the Creeps and the probe (native against sandbox, savestates,
  turbo, the clock, the inputs, the refusals). Each test says what it
  compared.
- `tests/run-frontend.sh --chimera-root <chimera>`: the package through
  `chimera-run` and through Chimera's window.
- Chimera's contract tests, against the package's folder:

      CHIMERA_CORES_DIR=<folder> <chimera>/build/meson-linux/source/ui/test-packages

The runners take a work folder holding what Chimera would mount: the pack,
a `slots` file (`{"pack":["game.pck"]}`) and a `settings` file. A movie for
them is a file of lines naming what is held each frame (`--movie`), or
`--press AT:TOKENS:LEN`; the names are waterbox.config's without spaces
(`KeyEnter`, `MouseLeftButton`, `P1A`) and axes are `mx=`, `my=`, `lx=`,
`ly=`, `rx=`, `ry=`, `lt=`, `rt=`.

## The test games

`tests/dodge-the-creeps.pck` and `tests/probe.pck` are remade by
`tests/make-test-game.sh <work folder>`, which downloads the Godot 4.6.3
editor and Godot's demo projects at a pinned commit. A remade pack is a new
test game: an export is not promised to be the same bytes twice.

## Troubleshooting

- "missing required export Init": the Chimera module was not linked whole.
- A crash in `lp_build_create_jit_compiler_for_module` in the sandbox: LLVM
  made no JIT; `jit-symbols.c` was not linked with `--wrap=dlopen`.
- A Godot build that stops in a few seconds with no error: `patches/` did
  not apply, or `build/godot-<flavour>/.chimera-source` is stale; remove
  that folder.
- `--main-pack ... compiled without support for path overrides`: export
  templates refuse it; the core finds the pack beside its executable
  instead (docs/PLAN.md).
