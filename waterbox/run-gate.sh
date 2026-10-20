#!/bin/bash
# run-gate.sh - the Godot core's tests: the native reference (run-native) and
# the sandbox (core.wbx through run-wbx) are the same machine, on two games
# in tests/ that anyone may have: Godot's "Dodge the Creeps" demo (MIT) and
# this repository's probe (tests/probe), which prints every frame what a game
# learns from the machine - the clock, the date, random numbers, the inputs.
#
# Every test says what it compared. The tests:
#   equivalence  run-native and run-wbx: every frame's picture, the sound,
#                each step's length and lag, the clock, and (the probe) every
#                line the game printed
#   rerecord     the sandbox saved and loaded before every frame = without
#   session      saved at a frame, a new host, loaded, finished = without
#   turbo        the first half not read back: the rest the same, and the
#                first half's pictures really not there
#   teeth        the comparison sees Enter pressed one frame later
#   clock        Godot's clock is frames: 1/60 s a frame, the date starts at
#                the Starting Clock setting and runs with the frames, and the
#                computer stalling between frames changes nothing
#   frame rate   at 30 frames a second a frame is 1/30 s, 1470 samples of
#                sound, and the core reports 30
#   keys         a key's event, with the character it types: Shift and A in
#                one frame type "A"
#   mouse        the mouse's place is a fraction of the window, a button's
#                press and release, a second click within 400 ms is a double
#                click and a later one is not, the wheel turns a notch a frame
#   controller   a button and the stick and trigger axes reach the game
#   save data    Export Save Data is what the game wrote to user:// (and
#                nothing of Godot's own), the same in both builds and after a
#                session; a project's save file, or the export's .zip, is in
#                user:// when the game starts; a zip that names a place
#                outside user:// is refused
#   renderer     the game draws on llvmpipe with the pinned CPU's 128-bit
#                vectors, in both builds, whatever the computer's CPU or
#                environment
#   version      a game of another Godot version is refused, naming the core
#                to use (as a .pck and embedded in an executable); a game of
#                this core's own version is not
#   refusals     a pack that is not there, and a file that is not a pack:
#                each says why
#
# usage: run-gate.sh -m <miniBox checkout>
set -u
here="$(cd "$(dirname "$0")" && pwd)"
root="$(cd "$here/.." && pwd)"
mb="${MINIBOX_DIR:-}"
while getopts "m:" opt; do
	case "$opt" in
		m) mb="$OPTARG" ;;
		*) exit 2 ;;
	esac
done
[ -n "$mb" ] && [ -d "$mb" ] || { echo "pass -m <miniBox checkout>" >&2; exit 2; }
mb="$(cd "$mb" && pwd)"

echo "== build (miniBox $mb)"
mkdir -p "$root/build"
step() { # step <what> <log> <command...>
	local what="$1" log="$2"; shift 2
	if ! "$@" > "$log" 2>&1; then
		grep "error:" "$log" | head -10; tail -15 "$log"; echo "$what failed ($log)"; exit 1
	fi
}
step "the LLVM builds" "$root/build/gate-llvm.log" bash -c "bash '$here/build-llvm.sh' native && bash '$here/build-llvm.sh' guest -m '$mb'"
step "the Mesa builds" "$root/build/gate-mesa.log" bash -c "bash '$here/setup-mesa.sh' -m '$mb' && bash '$here/setup-mesa.sh' -n"
step "the sandbox's math for the native build" "$root/build/gate-libm.log" bash "$here/make-native-libm.sh" "$mb"
step "the native Godot build" "$root/build/gate-native.log" bash "$here/build-godot.sh" native -m "$mb"
step "the guest Godot build" "$root/build/gate-guest.log" bash "$here/build-godot.sh" guest -m "$mb"
step "the native link" "$root/build/gate-link-native.log" bash "$here/link-core.sh" native -m "$mb"
step "the guest link" "$root/build/gate-link-guest.log" bash "$here/link-core.sh" guest -m "$mb"
native="$root/build/native/run-native"
wbx="$root/build/guest/run-wbx"
core="$root/build/guest/core.wbx"

work="$root/build/gate"
rm -rf "$work"
mkdir -p "$work"
passed=0
fails=0
pass() { echo "PASS $*"; passed=$((passed + 1)); }
fail() { echo "FAIL $*"; fails=$((fails + 1)); }

# what a run says: the digests and the lines the game printed (Godot's own
# warnings at exit, and miniBox's log lines, are not the machine's). A
# session test starts the game a second time in the new host before the
# state replaces it, and that start prints its start-up lines (renderer,
# ready) again: only the first of each counts.
digests() { grep -E '^([A-Za-z]+(\[[^]]*\])?=|probe )' "$1" | awk '!/^probe (ready|renderer)/ || !seen[$0]++' || true; }
same() { # same <test> <a> <b> <what>
	digests "$2" > "$2.d"
	digests "$3" > "$3.d"
	if [ -s "$2.d" ] && cmp -s "$2.d" "$3.d"; then
		pass "$1: $4"
	else
		fail "$1: $4"; diff "$2.d" "$3.d" | head -20
	fi
}
value() { digests "$1" | sed -n "s/^$2=//p"; }
has() { # has <test> <file> <fixed line> <what>
	if grep -qF -- "$3" "$2"; then pass "$1: $4"; else fail "$1: $4 (no line '$3')"; grep '^probe' "$2" | head -5; fi
}
hasnt() { # hasnt <test> <file> <fixed line> <what>
	if grep -qF -- "$3" "$2"; then fail "$1: $4 (found '$3')"; else pass "$1: $4"; fi
}
mkwork() { # mkwork <dir> <pack> [<settings JSON>]
	mkdir -p "$work/$1"
	cp "$2" "$work/$1/"
	printf '{"pack":["%s"]}' "$(basename "$2")" > "$work/$1/slots"
	printf '%s' "${3:-{\}}" > "$work/$1/settings"
}

# ---------------------------------------------------------------- Dodge the Creeps
frames=600
dtc_movie=(--press 130:KeyEnter:2 --press 200:KeyLeft:60 --press 260:KeyUp:60 --press 330:KeyRight:40 --press 420:mx=12000,my=50000:1)
mkwork dtc "$root/tests/dodge-the-creeps.pck"
echo "== Dodge the Creeps ($frames frames: Enter starts the game, the arrows move the player)"
"$native" "$work/dtc" --frames $frames "${dtc_movie[@]}" > "$work/n.txt" 2> "$work/n.err"
"$wbx" "$core" "$work/dtc" --frames $frames "${dtc_movie[@]}" > "$work/w.txt" 2> "$work/w.err"
same equivalence "$work/n.txt" "$work/w.txt" "native = sandbox, $frames frames: every picture, the sound, the steps"
"$wbx" "$core" "$work/dtc" --frames $frames "${dtc_movie[@]}" --rerecord > "$work/r.txt" 2> "$work/r.err"
same rerecord "$work/r.txt" "$work/w.txt" "save+load before every frame = straight ($(sed -n 's/stateBytes=//p' "$work/r.err") state bytes)"
"$wbx" "$core" "$work/dtc" --frames $frames "${dtc_movie[@]}" --session-at 350 > "$work/s.txt" 2> "$work/s.err"
same session "$work/s.txt" "$work/w.txt" "saved at frame 350 (in play), loaded into a new host = straight"

"$native" "$work/dtc" --frames $frames "${dtc_movie[@]}" --turbo > "$work/t.txt" 2> /dev/null
digests "$work/t.txt" | grep -v '^videoHash=' > "$work/t.d"
digests "$work/n.txt" | grep -v '^videoHash=' > "$work/n.d"
if cmp -s "$work/t.d" "$work/n.d" && [ "$(value "$work/t.txt" videoHash)" != "$(value "$work/n.txt" videoHash)" ]; then
	pass "turbo: the first half not read back - the second half's pictures, the sound and the steps the same; the first half's pictures not there"
else
	fail "turbo"; diff "$work/t.d" "$work/n.d" | head
fi

late=(--press 131:KeyEnter:2 --press 200:KeyLeft:60 --press 260:KeyUp:60 --press 330:KeyRight:40 --press 420:mx=12000,my=50000:1)
"$native" "$work/dtc" --frames $frames "${late[@]}" > "$work/late.txt" 2> /dev/null
if [ "$(value "$work/late.txt" videoHash)" != "$(value "$work/n.txt" videoHash)" ]; then
	pass "teeth: Enter one frame later is another run (the comparison sees it)"
else
	fail "teeth: Enter one frame later gave the same pictures"
fi

# ---------------------------------------------------------------- the probe
echo "== the probe (tests/probe)"
mkwork probe "$root/tests/probe.pck"
pf=130
probe_movie=(
	--press 3:KeyA,KeyLeftShift:2
	--press 5:mx=10000,my=20000:1
	--press 20:MouseLeftButton:1 --press 30:MouseLeftButton:1 --press 100:MouseLeftButton:1
	--press 40:MouseWheelUp:2
	--press 50:P1A,lx=32767,rt=16000:2 --press 60:lx=0,rt=0:1
)
"$native" "$work/probe" --frames $pf "${probe_movie[@]}" > "$work/pn.txt" 2> "$work/pn.err"
"$wbx" "$core" "$work/probe" --frames $pf "${probe_movie[@]}" > "$work/pw.txt" 2> "$work/pw.err"
same equivalence "$work/pn.txt" "$work/pw.txt" "the probe: native = sandbox, $pf frames, every picture, the sound and the $(grep -c '^probe' "$work/pn.txt") lines it printed"
"$wbx" "$core" "$work/probe" --frames $pf "${probe_movie[@]}" --session-at 25 > "$work/ps.txt" 2> "$work/ps.err"
same session "$work/ps.txt" "$work/pw.txt" "the probe saved at frame 25, between the two clicks, loaded into a new host = straight"

has renderer "$work/pw.txt" "probe renderer=[llvmpipe (LLVM 18.1.8, 128 bits)] [4.5 (Core Profile) Mesa 24.0.9]" "the game draws on llvmpipe with 128-bit vectors (the pinned CPU) in the sandbox"
has renderer "$work/pn.txt" "probe renderer=[llvmpipe (LLVM 18.1.8, 128 bits)] [4.5 (Core Profile) Mesa 24.0.9]" "and in the native build, whatever this computer's CPU"
LP_NATIVE_VECTOR_WIDTH=256 "$native" "$work/probe" --frames 2 > "$work/pn256.txt" 2> /dev/null
has renderer "$work/pn256.txt" "128 bits" "the native build ignores this computer's environment (LP_NATIVE_VECTOR_WIDTH=256 was set)"
has clock "$work/pn.txt" "probe ready unix=946684800 date=2000-01-01T00:00:00 ticks=0" "the game starts on 2000-01-01, the default Starting Clock, at time 0"
has clock "$work/pn.txt" "probe f=60 ticks=1000000 delta=0.016667 unix=946684801.000" "after 60 frames a second has gone by, and each frame is 1/60 s"
GATE_STALL_US=30000 "$native" "$work/probe" --frames 60 "${probe_movie[@]}" > "$work/stall.txt" 2> /dev/null
"$native" "$work/probe" --frames 60 "${probe_movie[@]}" > "$work/nostall.txt" 2> /dev/null
same clock "$work/stall.txt" "$work/nostall.txt" "the computer waiting 30 ms before every frame changes nothing the game sees"
mkwork probe2000 "$root/tests/probe.pck" '{"startTime": 1700000000}'
"$native" "$work/probe2000" --frames 2 > "$work/p2000.txt" 2> /dev/null
has clock "$work/p2000.txt" "probe ready unix=1700000000 date=2023-11-14T22:13:20" "Starting Clock 1700000000 is the date the game reads"

mkwork probe30 "$root/tests/probe.pck" '{"frameRate": "30"}'
"$native" "$work/probe30" --frames 30 --audio "$work/p30.raw" > "$work/p30.txt" 2> /dev/null
if grep -qF "probe f=30 ticks=1000000 delta=0.033333" "$work/p30.txt" && [ "$(value "$work/p30.txt" vsync)" = "30/1" ] &&
   [ "$(stat -c %s "$work/p30.raw")" = $((30 * 1470 * 4)) ]; then
	pass "frame rate: at 30 a frame is 1/30 s (30 frames make a second), 1470 stereo samples of sound, and the core says 30/1"
else
	fail "frame rate: $(grep -m1 'probe f=30 ' "$work/p30.txt"), vsync $(value "$work/p30.txt" vsync), $(stat -c %s "$work/p30.raw") bytes of sound"
fi

has keys "$work/pn.txt" "probe event f=3 key Shift down unicode=0 shift=true" "Shift goes down first"
has keys "$work/pn.txt" "probe event f=3 key A down unicode=65 shift=true" "A pressed with it types 'A'"
has keys "$work/pn.txt" "probe event f=5 key A up unicode=0 shift=true" "A comes up before Shift"
has mouse "$work/pn.txt" "probe event f=5 mouse moved to (48.0, 73.0) by (-112.0, -47.0)" "the mouse at 10000,20000 of 65535 is pixel 48,73 of the 320x240 window, moved from its middle"
has mouse "$work/pn.txt" "probe event f=20 mouse button 1 down at (48.0, 73.0) double=false" "a click"
has mouse "$work/pn.txt" "probe event f=30 mouse button 1 down at (48.0, 73.0) double=true" "a second click 10 frames later is a double click"
has mouse "$work/pn.txt" "probe event f=100 mouse button 1 down at (48.0, 73.0) double=false" "a click 70 frames later is not"
if [ "$(grep -c 'probe event f=4[01] mouse button 4 down' "$work/pn.txt")" = 2 ]; then
	pass "mouse: the wheel held for 2 frames turns 2 notches up (button 4)"
else
	fail "mouse: the wheel: $(grep 'mouse button 4' "$work/pn.txt" | head -3)"
fi
has controller "$work/pn.txt" "probe event f=50 pad button 0 down" "P1 A is the controller's button 0 (JOY_BUTTON_A)"
has controller "$work/pn.txt" "probe event f=50 pad axis 0 1.0000" "the left stick all the way right is 1"
has controller "$work/pn.txt" "probe event f=50 pad axis 5 0.4883" "the right trigger at 16000 of 32767 is 0.4883"
has controller "$work/pn.txt" "probe f=62 ticks=1033333 delta=0.016667 unix=946684801.033 rnd=" "the frames go on after the controller is let go"
has user:// "$work/pn.txt" "saved=[]" "the game's user:// is empty when the machine starts"

echo "== save data (the probe keeps user://probe.txt and user://saves/slot.dat)"
mkdir -p "$work/sdn" "$work/sdw" "$work/sds"
"$native" "$work/probe" --frames 30 --savedata-out "$work/sdn" > "$work/sdn.txt" 2> /dev/null
"$wbx" "$core" "$work/probe" --frames 30 --savedata-out "$work/sdw" > "$work/sdw.txt" 2> /dev/null
"$wbx" "$core" "$work/probe" --frames 30 --session-at 10 --savedata-out "$work/sds" > "$work/sds.txt" 2> /dev/null
if [ "$(cat "$work/sdn/probe.txt" 2>/dev/null)" = "written at unix 946684800" ] && [ "$(cat "$work/sdn/saves/slot.dat" 2>/dev/null)" = "slot of frame 0" ] &&
   [ "$(cd "$work/sdn" && find . -type f | sort | xargs cat | sha1sum)" = "$(cd "$work/sdw" && find . -type f | sort | xargs cat | sha1sum)" ] &&
   diff -r "$work/sdw" "$work/sds" > /dev/null && [ "$(find "$work/sdn" -type f | wc -l)" = 2 ]; then
	pass "save data: Export Save Data has the game's two files, one in a folder, and nothing else (no log, no shader cache); native = sandbox = a session"
else
	fail "save data: export: $(cd "$work/sdn" 2>/dev/null && find . -type f | sort | tr '\n' ' ')"
fi
seed() { # seed <dir> <slots JSON> <file to bring>...
	local d="$work/$1" slots="$2"; shift 2
	mkdir -p "$d"
	cp "$root/tests/probe.pck" "$d/"
	printf '%s' "$slots" > "$d/slots"
	printf '{}' > "$d/settings"
	for f in "$@"; do cp "$f" "$d/"; done
}
printf 'hello from a project' > "$work/probe.txt"
seed seed1 '{"pack":["probe.pck"],"savedata":["probe.txt"]}' "$work/probe.txt"
"$native" "$work/seed1" --frames 2 > "$work/seed1.txt" 2> /dev/null
has "save data" "$work/seed1.txt" "saved=[hello from a project] slot=[]" "a project's save file is in user:// when the game starts"
( cd "$work/sdw" && python3 -c "
import os, zipfile
z = zipfile.ZipFile('../user.zip', 'w')
for root, dirs, files in sorted(os.walk('.')):
    for f in sorted(files):
        z.write(os.path.join(root, f), os.path.relpath(os.path.join(root, f), '.'))
" )
seed seedzip '{"pack":["probe.pck"],"savedata":["user.zip"]}' "$work/user.zip"
"$native" "$work/seedzip" --frames 2 > "$work/seedzip.txt" 2> /dev/null
"$wbx" "$core" "$work/seedzip" --frames 2 > "$work/seedzipw.txt" 2> /dev/null
has "save data" "$work/seedzip.txt" "saved=[written at unix 946684800] slot=[slot of frame 0]" "the export, zipped as Chimera zips several files, goes back with its folder"
same "save data" "$work/seedzip.txt" "$work/seedzipw.txt" "the zip read back: native = sandbox"
python3 -c "
import zipfile
z = zipfile.ZipFile('$work/bad.zip', 'w'); z.writestr('../escape.txt', 'no'); z.close()"
mkdir -p "$work/seedbad"; cp "$work/bad.zip" "$work/seedbad/user.zip"
seed seedbad '{"pack":["probe.pck"],"savedata":["user.zip"]}'
"$native" "$work/seedbad" --frames 1 > "$work/seedbad.txt" 2> /dev/null
has "save data" "$work/seedbad.txt" "loadError=the save data zip user.zip holds ../escape.txt, which is not a name in user://" "a zip holding a name outside user:// is refused, saying which"

echo "== the game's Godot version"
# A pack says which Godot exported it; a core runs its own version's games
# (the 4.4 core also the older ones) and names the core for any other. The
# packs here are only a header, which is all the check reads, as a .pck and
# embedded at the end of an executable.
minor="$(sed -n 's/^minor = //p' "$root/extern/godot/version.py")"
mkpack() { # mkpack <dir> <file> <major> <minor> [embedded]
	mkdir -p "$work/$1"
	python3 - "$work/$1/$2" "$3" "$4" "${5:-}" <<'PY'
import struct, sys
path, major, minor, emb = sys.argv[1], int(sys.argv[2]), int(sys.argv[3]), sys.argv[4] == "embedded"
pack = struct.pack("<4sIIII", b"GDPC", 3, major, minor, 1) + b"\0" * 64
data = (b"\x7fELF" + b"\0" * 1000 + pack + struct.pack("<Q", len(pack)) + b"GDPC") if emb else pack
open(path, "wb").write(data)
PY
	printf '{"pack":["%s"]}' "$2" > "$work/$1/slots"
	printf '{}' > "$work/$1/settings"
}
other=$((minor == 7 ? 6 : 7))
mkpack vother game.pck 4 "$other"
"$native" "$work/vother" --frames 1 > "$work/vother.txt" 2> /dev/null
has version "$work/vother.txt" "loadError=this game was made with Godot 4.$other.1: open it with the Godot 4.$other core (this one runs Godot 4.$minor games)" "a game of Godot 4.$other is refused, naming its core"
mkpack vemb game.x86_64 4 "$other" embedded
"$wbx" "$core" "$work/vemb" --frames 1 > "$work/vemb.txt" 2> /dev/null
has version "$work/vemb.txt" "loadError=this game was made with Godot 4.$other.1: open it with the Godot 4.$other core" "and so is one embedded in its executable, in the sandbox"
mkpack v3 game.pck 3 5
"$native" "$work/v3" --frames 1 > "$work/v3.txt" 2> /dev/null
has version "$work/v3.txt" "loadError=this game was made with Godot 3.5.1; the Godot cores run games of Godot 4" "a Godot 3 game is refused"
mkpack vold game.pck 4 2
"$native" "$work/vold" --frames 1 > "$work/vold.txt" 2> /dev/null
if [ "$minor" -le 4 ]; then
	hasnt version "$work/vold.txt" "this game was made with Godot" "a Godot 4.2 game is taken by this core, the oldest"
else
	has version "$work/vold.txt" "loadError=this game was made with Godot 4.2.1: open it with the Godot 4.4 core, which also runs games of older Godot 4 versions" "a Godot 4.2 game is sent to the 4.4 core"
fi
mkpack vown game.pck 4 "$minor"
"$native" "$work/vown" --frames 1 > "$work/vown.txt" 2> /dev/null
hasnt version "$work/vown.txt" "this game was made with Godot" "a game of this core's own version is not refused for its version"

echo "== refusals"
mkdir -p "$work/nopack"
printf '{"pack":["missing.pck"]}' > "$work/nopack/slots"; printf '{}' > "$work/nopack/settings"
"$native" "$work/nopack" --frames 1 > "$work/nopack.txt" 2> /dev/null
if grep -qF "loadError=the game's pack (missing.pck) could not be opened" "$work/nopack.txt"; then
	pass "refusals: a pack that is not there says so"
else
	fail "refusals: missing pack: $(grep loadError "$work/nopack.txt")"
fi
mkdir -p "$work/notpack"
head -c 4096 /dev/zero | tr '\0' 'x' > "$work/notpack/game.pck"
printf '{"pack":["game.pck"]}' > "$work/notpack/slots"; printf '{}' > "$work/notpack/settings"
"$native" "$work/notpack" --frames 1 > "$work/notpack.txt" 2> /dev/null
if grep -q "^loadError=Godot could not start this game" "$work/notpack.txt"; then
	pass "refusals: a file that is not a Godot pack: $(sed -n 's/^loadError=//p' "$work/notpack.txt")"
else
	fail "refusals: not a pack: $(grep loadError "$work/notpack.txt")"
fi

echo
echo "$passed passed, $fails failed"
[ "$fails" -eq 0 ]
