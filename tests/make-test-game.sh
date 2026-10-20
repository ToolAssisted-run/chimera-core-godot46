#!/bin/bash
# Remakes the two games the tests play, exported to .pck files by the Godot
# 4.6.3 editor for Linux, with the export preset below:
#   tests/dodge-the-creeps.pck  Godot's own "Dodge the Creeps" demo
#                               (godot-demo-projects, MIT licensed, the licence
#                               is tests/dodge-the-creeps.LICENSE.md);
#   tests/probe.pck             this repository's probe (tests/probe), which
#                               prints what a game learns from the machine.
# The .pck files in the repository are the ones the tests were made against;
# an export is not promised to come out byte for byte the same twice, so a
# remade one is a new test game.
#
# Usage: tests/make-test-game.sh <work folder>   (downloads about 80 MB)
set -euo pipefail

here="$(cd "$(dirname "$0")" && pwd)"
work="${1:?usage: $0 <work folder>}"
mkdir -p "$work"
work="$(cd "$work" && pwd)"

editor_url="https://github.com/godotengine/godot/releases/download/4.6.3-stable/Godot_v4.6.3-stable_linux.x86_64.zip"
editor_sha256="d0bc2113065e481c9c2c2b2c37daa4e8be3fe9e27f0ab9ab0b6096e9a37907f3"
demos_commit="b761b4cd5718a7118f2c95eb55f181d0a5f004f1"

cd "$work"
[ -f editor.zip ] || curl -fL --retry 3 -o editor.zip "$editor_url"
echo "$editor_sha256  editor.zip" | sha256sum -c - > /dev/null
unzip -q -o editor.zip
editor="$work/Godot_v4.6.3-stable_linux.x86_64"

if [ ! -d demos ]; then
	git clone -q https://github.com/godotengine/godot-demo-projects.git demos
fi
git -C demos checkout -q "$demos_commit"
rm -rf dtc probe
cp -r demos/2d/dodge_the_creeps dtc
cp -r "$here/probe" probe
cat > dtc/export_presets.cfg <<'PRESET'
[preset.0]

name="Linux"
platform="Linux"
runnable=true
dedicated_server=false
custom_features=""
export_filter="all_resources"
include_filter=""
exclude_filter=""
export_path=""
encryption_include_filters=""
encryption_exclude_filters=""
encrypt_pck=false
encrypt_directory=false

[preset.0.options]

binary_format/embed_pck=false
texture_format/s3tc_bptc=true
texture_format/etc2_astc=false
binary_format/architecture="x86_64"
PRESET
cp dtc/export_presets.cfg probe/
( cd dtc && "$editor" --headless --import > ../import.log 2>&1 && "$editor" --headless --export-pack "Linux" "$here/dodge-the-creeps.pck" > ../export.log 2>&1 )
( cd probe && "$editor" --headless --import > ../probe-import.log 2>&1 && "$editor" --headless --export-pack "Linux" "$here/probe.pck" > ../probe-export.log 2>&1 )
echo "wrote $here/dodge-the-creeps.pck and $here/probe.pck"
