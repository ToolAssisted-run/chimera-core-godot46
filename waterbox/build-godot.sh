#!/usr/bin/env bash
# Builds Godot as static libraries with the Chimera module in it, for one of
# two machines:
#
#   native  the computer this runs on (glibc): the native reference, which
#           runs the same core outside the sandbox for the test scripts;
#   guest   miniBox's sandbox (musl, the guest kit's flags): the core.
#
# Godot builds with SCons and writes its objects and generated files beside
# its sources, so each flavour builds in its own copy of the pinned source
# (build/godot-<flavour>), made from the submodule's commit with the patches
# applied; the submodule itself is never written to.
#
# Usage: ./build-godot.sh native|guest [-j N] [-m <miniBox checkout>]
#   SCONS=<path>   an scons to use (default: one installed under build/tools)
#   SCONS_EXTRA    more arguments for scons (-k to see every error at once)
set -euo pipefail

here="$(cd "$(dirname "$0")" && pwd)"
root="$(cd "$here/.." && pwd)"
flavour="${1:-}"
shift || true
case "$flavour" in
	native | guest) ;;
	*)
		echo "usage: $0 native|guest [-j N] [-m <miniBox checkout>]" >&2
		exit 2
		;;
esac
jobs="$(nproc)"
minibox="${MINIBOX_DIR:-}"
while getopts "j:m:" opt; do
	case "$opt" in
		j) jobs="$OPTARG" ;;
		m) minibox="$OPTARG" ;;
		*) exit 2 ;;
	esac
done

godot="$root/extern/godot"
[ -f "$godot/SConstruct" ] || {
	echo "extern/godot is not checked out; run: git -C $root submodule update --init extern/godot" >&2
	exit 1
}
src="$root/build/godot-$flavour"

# --- SCons, in a folder of its own ------------------------------------------
scons="${SCONS:-}"
if [ -z "$scons" ]; then
	tools="$root/build/tools/scons"
	if [ ! -f "$tools/SCons/__init__.py" ]; then
		echo "build-godot: installing SCons into $tools"
		python3 -m pip install -q --target "$tools" "scons==4.11.1"
	fi
	scons="env PYTHONPATH=$tools python3 -m SCons"
fi

# --- the source copy ---------------------------------------------------------
# Remade when the submodule's commit or the patches change.
stamp="$(git -C "$godot" rev-parse HEAD) $( (cat "$root"/patches/*.patch 2>/dev/null || true) | sha1sum | cut -c1-40)"
if [ ! -f "$src/.chimera-source" ] || [ "$(cat "$src/.chimera-source")" != "$stamp" ]; then
	echo "build-godot: copying Godot $(git -C "$godot" describe --tags 2>/dev/null || true) into $src"
	rm -rf "$src"
	mkdir -p "$src"
	git -C "$godot" archive HEAD | tar -x -C "$src"
	for p in "$root"/patches/*.patch; do
		[ -f "$p" ] || continue
		echo "build-godot: applying $(basename "$p")"
		patch -d "$src" -p1 --quiet < "$p"
	done
	echo "$stamp" > "$src/.chimera-source"
fi

# --- the options -------------------------------------------------------------
# An exported game's engine (template_release) for Linux, as libraries; one
# thread; OpenGL only (the Compatibility renderer); nothing that talks to a
# desktop (X11, Wayland, sound servers, D-Bus, speech, fonts of the system,
# hot-plugged controllers, screen readers), and neither OpenXR nor cameras,
# which look for devices; PCRE2 without its JIT, whose code would depend on
# the CPU it runs on.
opts=(
	platform=linuxbsd target=template_release arch=x86_64
	library_type=static_library
	threads=no
	opengl3=yes vulkan=no use_volk=no d3d12=no metal=no angle=no
	x11=no wayland=no libdecor=no alsa=no pulseaudio=no dbus=no speechd=no
	fontconfig=no udev=no sdl=no accesskit=no touch=no
	builtin_pcre2_with_jit=no
	module_openxr_enabled=no module_camera_enabled=no
	custom_modules="$root/module"
	debug_symbols=no
	progress=no
	"-j$jobs"
)

kit_native="$here/native-shim"
# With no threads, Godot still names the main thread through pthreads, and
# its Linux code then lacks pthread.h; there is nothing to name.
common_flags="-DPTHREAD_NO_RENAME"
if [ "$flavour" = guest ]; then
	[ -n "$minibox" ] || {
		echo "build-godot: the guest build needs -m <miniBox checkout> (or MINIBOX_DIR)" >&2
		exit 1
	}
	minibox="$(cd "$minibox" && pwd)"
	sr="$minibox/build/meson-cpp/guest-sysroot"
	[ -f "$sr/lib/musl-gcc.specs" ] || {
		echo "build-godot: the C++ guest sysroot is not built at $sr (build miniBox's meson-cpp first)" >&2
		exit 1
	}
	gccver="$(basename "$(ls -d "$sr"/include/c++/* | head -1)")"

	# compilers that carry the guest specs inside them (SCons passes CC and
	# CXX around as single words in places)
	tools="$root/build/guest-tools"
	mkdir -p "$tools"
	printf '#!/bin/sh\nexec gcc "$@" -specs %s/lib/musl-gcc.specs\n' "$sr" > "$tools/gcc"
	printf '#!/bin/sh\nexec g++ "$@" -specs %s/lib/musl-gcc.specs\n' "$sr" > "$tools/g++"
	chmod +x "$tools/gcc" "$tools/g++"

	# kernel headers (linux/, asm/) after the guest's own, as setup-mesa.sh
	uapi="$root/build/guest-tools/uapi-include"
	mkdir -p "$uapi"
	ln -sfn /usr/include/linux "$uapi/linux"
	ln -sfn /usr/include/asm-generic "$uapi/asm-generic"
	if [ -d /usr/include/x86_64-linux-gnu/asm ]; then
		ln -sfn /usr/include/x86_64-linux-gnu/asm "$uapi/asm"
	else
		ln -sfn /usr/include/asm "$uapi/asm"
	fi

	guest_flags="$common_flags -mcmodel=large -mno-red-zone -mstack-protector-guard=global -fno-stack-protector -fno-pic -fno-pie -fcf-protection=none -DCHIMERA_GUEST -I$here/guest-compat -idirafter $uapi"
	opts+=(
		CC="$tools/gcc" CXX="$tools/g++"
		ccflags="$guest_flags"
		cxxflags="-I$sr/include/c++/$gccver -I$sr/include/c++/$gccver/x86_64-linux-musl"
		use_static_cpp=no
		extra_suffix=guest
		chimera_kit="$minibox/extern/emulibc:$minibox/source/guest/include:$minibox/extern/jsmn"
	)
else
	[ -n "$minibox" ] && [ -d "$minibox/source/guest/include" ] || {
		echo "build-godot: the native build reads the guest kit's settings headers; pass -m <miniBox checkout>" >&2
		exit 1
	}
	minibox="$(cd "$minibox" && pwd)"
	opts+=(
		ccflags="$common_flags"
		extra_suffix=native
		chimera_kit="$kit_native:$minibox/source/guest/include:$minibox/extern/jsmn"
	)
fi

cd "$src"
echo "build-godot: scons ${opts[*]}"
log="$root/build/scons-$flavour.log"
set +e
$scons "${opts[@]}" ${SCONS_EXTRA:-} 2>&1 | tee "$log"
status=${PIPESTATUS[0]}
set -e
if [ "$status" != 0 ]; then
	# Before 4.6 Godot has no library_type: it links a Godot program after
	# the libraries, which the core does not use and which cannot link for
	# the sandbox. When that link is the only thing that failed, the
	# libraries are all there.
	failed="$(grep '^scons: \*\*\* \[' "$log" | grep -v '^scons: \*\*\* \[bin/godot\.linuxbsd\.' || true)"
	if [ -n "$failed" ] || ! grep -q '^scons: \*\*\* \[bin/godot\.linuxbsd\.' "$log"; then
		echo "build-godot: the build failed" >&2
		exit 1
	fi
	echo "build-godot: only Godot's own program did not link (Godot before 4.6 always makes one); the libraries are built"
fi
echo "build-godot: $flavour libraries in $src"
