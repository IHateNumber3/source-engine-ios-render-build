#!/bin/bash
# Builds the engine for iOS (device, arm64, togles+ANGLE) via waf, assembles it into
# a .app bundle, ad-hoc codesigns it, and zips it into an unsigned .ipa.
#
# This only packages the ENGINE. It ships no game content (maps/materials/models are
# Valve's copyrighted assets, not part of this repo) -- the resulting app expects the
# user to copy their own game folders (e.g. "hl2", "portal", "platform") onto the
# device via the Files app / Finder file sharing, matching how this engine already
# resolves content from Documents/<mod>/ at runtime (see AppFramework's LoadModule
# search order).
#
# Requires: scripts/ios/fetch-angle.sh and scripts/ios/build-deps.sh already run.
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
GAME="${GAME:-hl2}"
BUNDLE_NAME="${BUNDLE_NAME:-SourceEngine}"
BUNDLE_ID="${BUNDLE_ID:-com.v0idx9.sourceengine.$GAME}"
BUILD_VARIANT="${BUILD_VARIANT:-release}"

FRAMEWORKS_DIR="$ROOT_DIR/Frameworks"
SDK_ROOT="$(xcrun --sdk iphoneos --show-sdk-path)"
SDK_FRAMEWORKS_DIR="$SDK_ROOT/System/Library/Frameworks"

echo "=== Sanity checks ==="
for f in "$FRAMEWORKS_DIR/SDL2.framework/SDL2" "$FRAMEWORKS_DIR/libEGL.framework/libEGL" "$FRAMEWORKS_DIR/libGLESv2.framework/libGLESv2" "$ROOT_DIR/lib/darwin/aarch64/libz.a"; do
	if [ ! -e "$f" ]; then
		echo "error: missing $f -- run fetch-angle.sh and build-deps.sh first" >&2
		exit 1
	fi
done

# wscript has no CLI option to point ANGLE at a custom framework search path (unlike
# SDL2, which has --sdl2=<path>), so the only way to make clang's default -isysroot
# framework search find libEGL/libGLESv2 is to place them where it already looks:
# inside the SDK bundle. This only touches the ephemeral CI runner's Xcode copy.
echo "=== Installing ANGLE frameworks into SDK ($SDK_FRAMEWORKS_DIR) ==="
sudo cp -R "$FRAMEWORKS_DIR/libEGL.framework" "$SDK_FRAMEWORKS_DIR/"
sudo cp -R "$FRAMEWORKS_DIR/libGLESv2.framework" "$SDK_FRAMEWORKS_DIR/"

PAYLOAD_DIR="$ROOT_DIR/Payload"
APP_DIR="$PAYLOAD_DIR/$BUNDLE_NAME.app"
rm -rf "$PAYLOAD_DIR"
mkdir -p "$APP_DIR"

# The ivp (physics) submodule predates this iOS port and only ever checks
# "#ifdef OSX" (this codebase's macOS-only define; wscript sets IOS/_IOS instead of
# OSX/_OSX for iOS builds) to pick <malloc/malloc.h> vs Linux's <malloc.h>, which
# doesn't exist on iOS either ("fatal error: 'malloc.h' file not found"). It's a
# separate repo (nillerusr/source-physics) we can't push a fix to directly, so patch
# every occurrence of this exact pattern transiently, same as the thirdparty patches
# in build-deps.sh. All 4 known instances share the identical structure.
if [ -d "$ROOT_DIR/ivp" ]; then
	# Multiple --include patterns on macOS's BSD grep only reliably honored one of
	# them in practice (missed ivu_types.hxx -- the actual file causing the build
	# failure -- while matching .cxx files fine), so just grep everything under ivp/
	# instead of trying to filter by extension.
	# The "#ifdef OSX" line isn't always at column 0 either -- ivu_types.hxx has it
	# nested ("#   ifdef OSX", indented 3 spaces, inside an outer #if), while the
	# other 3 files have it unindented. Match either.
	IVP_MALLOC_FILES="$(grep -rlE '^#[[:space:]]*ifdef OSX[[:space:]]*$' "$ROOT_DIR/ivp" 2>/dev/null || true)"
	for f in $IVP_MALLOC_FILES; do
		if grep -A1 -E '^#[[:space:]]*ifdef OSX[[:space:]]*$' "$f" | grep -q 'include <malloc/malloc.h>'; then
			echo "=== Patching $f: also use <malloc/malloc.h> on iOS (not just OSX) ===" >&2
			sed -i '' -E 's/^#([[:space:]]*)ifdef OSX[[:space:]]*$/#\1if defined(OSX) || defined(IOS)/' "$f"
		fi
	done

	# Same underlying issue, different header: several ivp files only include
	# <alloca.h> under "#if defined(LINUX) || defined(SUN) || (__MWERKS__ && ...)",
	# never checking for any Apple platform, so alloca() is undeclared on iOS. Unlike
	# the malloc.h fix above, the exact condition line's formatting is wildly
	# inconsistent across these files (tabs vs spaces, defined(__MWERKS__) vs bare
	# __MWERKS__, spaced vs unspaced ||), so rewriting that line reliably with a
	# single regex isn't practical. Prepend #include <stdlib.h> instead of a
	# standalone <alloca.h> -- stdlib.h is a standard header guaranteed to exist
	# everywhere (unlike malloc.h/alloca.h, which are platform-specific and where
	# assuming either exists on iOS without checking is exactly the mistake this
	# whole ivp patching keeps correcting), and Darwin's stdlib.h declares alloca()
	# as a BSD extension. This patch is transient/CI-only, never committed.
	IVP_ALLOCA_FILES="$(grep -rl 'include <alloca.h>' "$ROOT_DIR/ivp" 2>/dev/null || true)"
	for f in $IVP_ALLOCA_FILES; do
		if ! head -1 "$f" | grep -q '#include <stdlib.h>'; then
			echo "=== Patching $f: unconditionally provide <stdlib.h> for alloca() (undeclared on iOS otherwise) ===" >&2
			TMP_ALLOCA="$(mktemp)"
			{ echo '#include <stdlib.h> /* patched by scripts/ios/package-ipa.sh, see comment there: alloca() for iOS */'; cat "$f"; } > "$TMP_ALLOCA"
			mv "$TMP_ALLOCA" "$f"
		fi
	done
fi

echo "=== Configuring waf (game=$GAME) ==="
cd "$ROOT_DIR"
./waf configure -T "$BUILD_VARIANT" \
	--ios --togles --angle \
	--sdl2="$FRAMEWORKS_DIR/SDL2.framework" \
	--build-games="$GAME" \
	--prefix="$APP_DIR"

echo "=== Building ==="
./waf build

echo "=== Installing build outputs into $APP_DIR ==="
./waf install

if [ ! -f "$APP_DIR/hl2_launcher" ]; then
	echo "error: waf install did not produce $APP_DIR/hl2_launcher" >&2
	echo "contents of $APP_DIR:" >&2
	ls -la "$APP_DIR" >&2
	exit 1
fi

echo "=== Fixing up internal dylib-to-dylib install names ==="
# This wscript never loads waf's c_osx tool (which normally auto-generates relocatable
# @rpath install names for shared libraries on Apple platforms), so every dylib waf
# links here keeps clang's raw default: its own absolute build-directory path baked
# into its own ID, and every OTHER dylib that links against it captures that same
# absolute path verbatim. That's fine on the CI runner where the path briefly exists,
# but it can never resolve on an actual device. Confirmed via a real device's
# boot_diag.log: liblauncher.dylib failed to dlopen with
# "Library not loaded: /Users/runner/work/source-engine/source-engine/build/togles/libtogl.dylib".
# liblauncher->libtogl is just the one that happened to be discovered first (loading
# order stops there) -- every other dylib-to-dylib link in this ~30-dylib flat bundle
# is equally likely to have the exact same problem, just not yet reached, so fix all
# of them in one generic pass rather than one at a time as each surfaces.
#
# All these dylibs (and the main executable) sit flat in the same directory
# ($APP_DIR, since PREFIX=BINDIR=LIBDIR=$APP_DIR for iOS), so @loader_path -- "next to
# whichever binary is doing the loading" -- resolves correctly for every single one of
# them uniformly, regardless of which dylib depends on which or how deep the chain is.

# Give every dylib a clean, relocatable self-identity first.
for dylib in "$APP_DIR"/*.dylib; do
	[ -f "$dylib" ] || continue
	install_name_tool -id "@rpath/$(basename "$dylib")" "$dylib"
done

# Then rewrite every binary's references to any of THOSE dylibs, wherever the
# reference doesn't already look relocatable, and make sure @loader_path is in that
# binary's own rpath list so @rpath/... actually resolves.
for bin in "$APP_DIR/hl2_launcher" "$APP_DIR"/*.dylib; do
	[ -f "$bin" ] || continue
	needs_rpath=0
	while IFS= read -r dep_line; do
		dep_path="$(echo "$dep_line" | awk '{print $1}')"
		[ -z "$dep_path" ] && continue
		case "$dep_path" in
			@rpath/*|@executable_path/*|@loader_path/*) continue ;;
		esac
		dep_base="$(basename "$dep_path")"
		[ -f "$APP_DIR/$dep_base" ] || continue
		install_name_tool -change "$dep_path" "@rpath/$dep_base" "$bin"
		needs_rpath=1
	done < <(otool -L "$bin" 2>/dev/null | tail -n +2)
	if [ "$needs_rpath" = "1" ]; then
		install_name_tool -add_rpath "@loader_path" "$bin" 2>/dev/null || true
	fi
done

echo "=== Writing Info.plist ==="
sed -e "s/__BUNDLE_ID__/$BUNDLE_ID/g" -e "s/__BUNDLE_NAME__/$BUNDLE_NAME/g" \
	"$ROOT_DIR/launcher_main/ios/Info.plist" > "$APP_DIR/Info.plist"

echo "=== Embedding frameworks ==="
mkdir -p "$APP_DIR/Frameworks"
cp -R "$FRAMEWORKS_DIR/SDL2.framework" "$APP_DIR/Frameworks/"
cp -R "$FRAMEWORKS_DIR/libEGL.framework" "$APP_DIR/Frameworks/"
cp -R "$FRAMEWORKS_DIR/libGLESv2.framework" "$APP_DIR/Frameworks/"

# ANGLE loads its GLES/EGL backends as further dylibs beside libGLESv2/libEGL in the
# same release archive on some ANGLE builds; harmless no-op if there are none.
shopt -s nullglob
for extra in "$FRAMEWORKS_DIR"/*.framework; do
	base="$(basename "$extra")"
	if [ ! -d "$APP_DIR/Frameworks/$base" ]; then
		cp -R "$extra" "$APP_DIR/Frameworks/"
	fi
done
shopt -u nullglob

echo "=== Fixing up install names and rpaths so the dynamic linker can find embedded frameworks ==="
# The prebuilt ANGLE frameworks (libEGL/libGLESv2) weren't built by this script, so
# unlike the hand-wrapped SDL2.framework (built with -install_name @rpath/... above),
# their own baked-in install name is unknown here -- it could be an @rpath reference,
# or an absolute path from the machine that built them, which would never resolve on
# the CI runner or an end-user device even with the right rpath added. Force each
# framework's own ID to a clean @rpath form, and rewrite every consuming binary's
# reference to match, whatever that reference originally looked like.
for fw in SDL2 libEGL libGLESv2; do
	fw_bin="$APP_DIR/Frameworks/$fw.framework/$fw"
	[ -f "$fw_bin" ] || continue
	install_name_tool -id "@rpath/$fw.framework/$fw" "$fw_bin"
done

# Every binary in the flat bundle root can potentially be the one that links
# SDL2/libEGL/libGLESv2 directly (materialsystem/shaderapi, not necessarily the
# launcher). @executable_path always resolves to the main executable's directory
# even when patching a dylib, and every binary here lives in that same directory,
# so this is safe to apply broadly rather than guessing which one links what.
for bin in "$APP_DIR/hl2_launcher" "$APP_DIR"/*.dylib; do
	[ -f "$bin" ] || continue
	needs_rpath=0
	for fw in SDL2 libEGL libGLESv2; do
		old_ref="$(otool -L "$bin" 2>/dev/null | awk -v pat="$fw.framework/$fw" '$1 ~ pat {print $1; exit}')"
		if [ -n "$old_ref" ] && [ "$old_ref" != "@rpath/$fw.framework/$fw" ]; then
			install_name_tool -change "$old_ref" "@rpath/$fw.framework/$fw" "$bin"
		fi
		[ -n "$old_ref" ] && needs_rpath=1
	done
	if [ "$needs_rpath" = "1" ]; then
		install_name_tool -add_rpath "@executable_path/Frameworks" "$bin" 2>/dev/null || true
	fi
done

echo "=== Ad-hoc codesigning (unsigned/local signature; re-sign with AltStore, Sideloadly, or Xcode before installing) ==="
ENTITLEMENTS="$ROOT_DIR/launcher_main/ios/entitlements-adhoc.plist"

codesign --force --sign - --timestamp=none "$APP_DIR/Frameworks/SDL2.framework/SDL2"
codesign --force --sign - --timestamp=none "$APP_DIR/Frameworks/libEGL.framework/libEGL"
codesign --force --sign - --timestamp=none "$APP_DIR/Frameworks/libGLESv2.framework/libGLESv2"

for dylib in "$APP_DIR"/*.dylib; do
	[ -f "$dylib" ] || continue
	codesign --force --sign - --timestamp=none "$dylib"
done

codesign --force --sign - --timestamp=none --entitlements "$ENTITLEMENTS" "$APP_DIR/hl2_launcher"
codesign --force --sign - --timestamp=none --entitlements "$ENTITLEMENTS" "$APP_DIR"

echo "=== Verifying signature ==="
codesign --verify --deep --strict "$APP_DIR"

echo "=== Zipping IPA ==="
IPA_PATH="$ROOT_DIR/$BUNDLE_NAME-$GAME.ipa"
rm -f "$IPA_PATH"
cd "$ROOT_DIR"
zip -qr "$IPA_PATH" "$(basename "$PAYLOAD_DIR")"

echo "=== Done: $IPA_PATH ==="
