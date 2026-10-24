#!/usr/bin/env bash
# Builds netcoind and the Qt wallet on macOS with Homebrew and packages them
# as dist/netcoin-<ver>-macos-<arch>.dmg (NetCoin.app with netcoind inside).
# Used by the GitHub workflows; see doc/build-macos.md.
set -euo pipefail

NCVER="${1:-dev}"
JOBS="$(sysctl -n hw.ncpu)"
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
ARCH="$(uname -m)"
QT="$(brew --prefix qt)"
LOG="$ROOT/build-macos.log"
: > "$LOG"
fail() {
    if [ -n "${GITHUB_ACTIONS:-}" ]; then
        local msg
        msg="$( (grep -E 'error:|Error [0-9]|No such file|not found|Library not loaded|Reason:' "$LOG" | grep -v 'search path' | cut -c1-220 | tail -12; grep -A12 'Undefined symbols' "$LOG" | cut -c1-200 | head -14; echo '--- last lines ---'; grep -v -E '(clang|clang\+\+|install_name_tool|moc|uic|rcc) ' "$LOG" | tail -12 | cut -c1-220) | perl -pe 's/%/%25/g; s/\r//g; s/\n/%0A/g')"
        echo "::error title=macOS build failed::${msg:0:60000}"
    fi
    exit 1
}
trap fail ERR
exec > >(tee -a "$LOG") 2>&1

cd "$ROOT/src"
make -f makefile.osx -j"$JOBS" netcoind test_netcoin
./test_netcoin

mkdir -p "$ROOT/build-qt"
cd "$ROOT/build-qt"
"$QT/bin/qmake" ../netcoin-qt.pro USE_UPNP=1 USE_QRCODE=1 CONFIG+=release
make -j"$JOBS"

NAME="netcoin-$NCVER-macos-$ARCH"
DIST="$ROOT/dist/$NAME"
rm -rf "$DIST"
mkdir -p "$DIST"
cp -R netcoin.app "$DIST/NetCoin.app"
APP="$DIST/NetCoin.app"
cp "$ROOT/src/netcoind" "$APP/Contents/MacOS/"
# Copies libraries that macdeployqt cannot resolve (loaded via @loader_path or
# @rpath, e.g. boost_filesystem -> boost_atomic) and points the binaries at
# the bundled copies. Run before macdeployqt (so that it does not fail on
# them) and after it.
bundle_libs() {
BREWLIB="$(brew --prefix)/lib"
FW="$APP/Contents/Frameworks"
for pass in 1 2 3 4 5; do
    added=0
    FWBINS=""
    for fw in "$FW"/*.framework; do
        [ -f "$fw/Versions/Current/$(basename "$fw" .framework)" ] && FWBINS="$FWBINS $fw/Versions/Current/$(basename "$fw" .framework)"
    done
    for f in "$FW"/*.dylib "$APP/Contents/MacOS/"* "$APP/Contents/PlugIns"/*/*.dylib $FWBINS; do
        [ -f "$f" ] || continue
        for dep in $(otool -L "$f" | awk 'NR>1{print $1}' | grep -E '^(@loader_path|@rpath|/opt/homebrew|/usr/local)/' | grep -v '\.framework/' || true); do
            name="$(basename "$dep")"
            # otool -L also lists a library's own install name
            [ "$name" = "$(basename "$f")" ] && continue
            if [ ! -f "$FW/$name" ]; then
                src="$BREWLIB/$name"
                [ -f "$src" ] || src="$(find -L "$(brew --prefix)/opt/" -maxdepth 4 -name "$name" -print -quit)"
                [ -n "$src" ] || { echo "error: $name (needed by $f) not found"; false; }
                cp -L "$src" "$FW/$name"; chmod u+w "$FW/$name"
                install_name_tool -id "@executable_path/../Frameworks/$name" "$FW/$name" 2>/dev/null
                added=1
            fi
            case "$f" in
                "$FW"/*.framework/*) install_name_tool -change "$dep" "@executable_path/../Frameworks/$name" "$f" 2>/dev/null ;;
                "$FW"/*) install_name_tool -change "$dep" "@loader_path/$name" "$f" 2>/dev/null ;;
                *)       install_name_tool -change "$dep" "@executable_path/../Frameworks/$name" "$f" 2>/dev/null ;;
            esac
        done
    done
    [ "$added" = 0 ] && break
done
}
BREWLIB="$(brew --prefix)/lib"
FW="$APP/Contents/Frameworks"
mkdir -p "$FW"
bundle_libs
# copy Qt and all Homebrew libraries into the bundle and point the binaries at them
"$QT/bin/macdeployqt" "$APP" -executable="$APP/Contents/MacOS/netcoind" -always-overwrite || true
# drop plugins whose Qt frameworks were not deployed (e.g. the PDF image
# format plugin needs QtPdf); the wallet does not use them
for p in $(find "$APP/Contents/PlugIns" -name '*.dylib'); do
    for dep in $(otool -L "$p" | awk 'NR>1{print $1}' | grep '\.framework/' || true); do
        fwname="$(echo "$dep" | sed -E 's#.*/([^/]+\.framework)/.*#\1#')"
        case "$dep" in /System/*) continue ;; esac
        if [ ! -d "$FW/$fwname" ]; then echo "removing plugin $(basename "$p") (needs $fwname)"; rm -f "$p"; break; fi
    done
done
bundle_libs
echo "== checking the bundle"
# nothing may still refer to Homebrew
if otool -L "$APP/Contents/MacOS/"* $(find "$FW" "$APP/Contents/PlugIns" -name '*.dylib') $FWBINS | grep -E '^[[:space:]]*/(opt|usr/local)/'; then
    echo "error: the bundle still links to Homebrew libraries"; false
fi
# macdeployqt invalidates the signatures; Apple Silicon needs at least an ad-hoc one
echo "== signing"
# sign from the inside out (--deep does not cover extra executables such as
# netcoind in Contents/MacOS)
find "$APP/Contents/Frameworks" "$APP/Contents/PlugIns" -name '*.dylib' -type f -exec codesign --force --sign - {} \;
for fw in "$APP/Contents/Frameworks"/*.framework; do [ -d "$fw" ] && codesign --force --sign - "$fw"; done
codesign --force --sign - "$APP/Contents/MacOS/netcoind"
codesign --force --sign - "$APP"
codesign --verify --deep --strict --verbose=2 "$APP"
("$APP/Contents/MacOS/netcoind" -? > "$ROOT/dist/help.txt" || true); grep -q Usage "$ROOT/dist/help.txt"; head -3 "$ROOT/dist/help.txt"; rm -f "$ROOT/dist/help.txt"

echo "== smoke test"
# smoke test of the bundled binaries (nobody tests the Mac build by hand):
# the node starts, answers RPC and stops cleanly; the wallet window starts
SMOKE="$(mktemp -d)"
printf 'rpcuser=u\nrpcpassword=p\nlisten=0\nlistenonion=0\nconnect=0\ndnsseed=0\n' > "$SMOKE/netcoin.conf"
"$APP/Contents/MacOS/netcoind" -testnet -datadir="$SMOKE" -daemon
for i in $(seq 1 30); do
    "$APP/Contents/MacOS/netcoind" -testnet -datadir="$SMOKE" getinfo >/dev/null 2>&1 && break
    sleep 2
done
"$APP/Contents/MacOS/netcoind" -testnet -datadir="$SMOKE" getinfo | grep -E '"version"|"blocks"'
"$APP/Contents/MacOS/netcoind" -testnet -datadir="$SMOKE" stop
sleep 5
mkdir -p "$SMOKE/gui"
"$APP/Contents/MacOS/netcoin" -testnet -datadir="$SMOKE/gui" -splash=0 -connect=0 -dnsseed=0 &
GUIPID=$!
sleep 30
kill -0 "$GUIPID"          # still running after 30 s
grep -a "Done loading" "$SMOKE/gui/testnet/debug.log"
kill "$GUIPID"; wait "$GUIPID" || true
rm -rf "$SMOKE"

cp "$ROOT/COPYING" "$DIST/COPYING.txt"
cp "$ROOT/contrib/ci/README-macos.txt" "$DIST/README.txt"
ln -s /Applications "$DIST/Applications"
cd "$ROOT/dist"
hdiutil create -volname "NetCoin $NCVER" -srcfolder "$NAME" -ov -format UDZO "$NAME.dmg"
ls -la "$ROOT/dist"
trap - ERR
