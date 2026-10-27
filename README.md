# chimera-core-godot46

A core for [Chimera](https://github.com/ToolAssisted-run/chimera), the
frontend for tool-assisted speedruns, that plays games made with **Godot
4.6** (the [Godot Engine](https://godotengine.org)). It is Godot 4.6.3's
own engine for exported games, built for Chimera's sandbox, and produces one
file, `godot46.chimeraCore`, that goes in Chimera's `cores` folder.

Each Godot 4 version has its core, because a game's compiled scripts only
load in the version that exported them:
[4.7](https://github.com/ToolAssisted-run/chimera-core-godot),
[4.6](https://github.com/ToolAssisted-run/chimera-core-godot46),
[4.5](https://github.com/ToolAssisted-run/chimera-core-godot45),
[4.4](https://github.com/ToolAssisted-run/chimera-core-godot44) (which also
takes games of 4.0 to 4.3). A core refuses a game of another version and
says which core to use. This repository is chimera-core-godot's code with
Godot 4.6.3 pinned; everything about how it works is in that repository's
[docs/PLAN.md](https://github.com/ToolAssisted-run/chimera-core-godot/blob/main/docs/PLAN.md).

## Status

Published: each push to `main` that passes the tests publishes a `dev`
release, and a dated nightly is made once a day when `main` moved. Chimera's
Core Manager lists it.

- Godot 4.6 games written in GDScript, drawn with Godot's Compatibility
  renderer (OpenGL 3.3) on a software OpenGL inside the core (Mesa's
  llvmpipe, made to generate the same code on every computer).
- Tried: Godot's "Dodge the Creeps" demo and the core's probe game, both
  exported with the Godot 4.6.3 editor. The sandboxed core and a plain native
  build agree on every picture, the sound and everything the game printed;
  savestates land where the straight run does. No commercial game has been
  tried yet.
- Not there: games written in C# and games that need native extensions; the
  GPU.

## What you bring

The game's `.pck` file, or the game's executable when the `.pck` is inside
it. Files the game loads from beside its executable go in the project's
"Other files" slot; its saves in "Save data".

## Buttons and settings

The keyboard, the mouse and one game controller; Frame Rate, Starting Clock
and Godot's Log, as in [chimera-core-godot](https://github.com/ToolAssisted-run/chimera-core-godot#buttons-and-settings).

## Building and testing

[docs/BUILDING.md](docs/BUILDING.md) has every step; [docs/PLAN.md](docs/PLAN.md)
says what is particular to this version.

## Licence

This repository's own code is MIT, and so is Godot. The libraries inside
Godot are under their own permissive licences, listed in Godot's
`COPYRIGHT.txt`; the software OpenGL is Mesa (MIT) with LLVM (Apache 2.0
with the LLVM exception). The package carries every licence text. See
[LICENSE](LICENSE).
