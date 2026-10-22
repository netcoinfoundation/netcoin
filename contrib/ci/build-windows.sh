#!/usr/bin/env bash
# Builds netcoind.exe and netcoin.exe (Qt wallet) in an MSYS2 UCRT64 shell
# and collects everything needed to run them into dist/netcoin-<ver>-win64/.
# Used by the GitHub workflows; can also be run by hand in MSYS2.
set -euo pipefail

VERSION="${1:-dev}"
JOBS="$(nproc)"
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
P="${MINGW_PACKAGE_PREFIX:-mingw-w64-ucrt-x86_64}"
LOG="$ROOT/build-windows.log"
: > "$LOG"

log() { echo "== $*" | tee -a "$LOG"; }

fail() {
    # make the end of the log visible as an annotation in the Actions UI
    if [ -n "${GITHUB_ACTIONS:-}" ]; then
        local msg
        msg="$(grep -E 'error|Error|undefined|cannot find|No such' "$LOG" | tail -40 | sed 's/%/%25/g' | sed ':a;N;$!ba;s/\n/%0A/g')"
        echo "::error title=Windows build failed::${msg:0:60000}"
    fi
    exit 1
}
trap fail ERR

log "installing packages"
REQUIRED="toolchain boost openssl sqlite3 leveldb miniupnpc zlib qt5-base qt5-tools"
OPTIONAL="db qrencode libqrencode"
# the MSYS2 mirrors are sometimes slow; retry
for attempt in 1 2 3 4; do
    if pacman -S --noconfirm --needed make git $(for p in $REQUIRED; do echo "$P-$p"; done) >>"$LOG" 2>&1; then
        break
    fi
    [ $attempt = 4 ] && false
    echo "pacman failed, retrying ($attempt)" >>"$LOG"
    sleep 20
done
HAVE_BDB=0
HAVE_QR=0
for p in $OPTIONAL; do
    if pacman -S --noconfirm --needed "$P-$p" >>"$LOG" 2>&1; then
        case $p in db) HAVE_BDB=1;; *qrencode) HAVE_QR=1;; esac
    fi
done
log "Berkeley DB (wallet conversion): $HAVE_BDB, QR codes: $HAVE_QR"

log "building netcoind.exe"
cd "$ROOT/src"
make -f makefile.mingw -j"$JOBS" USE_BDB=$HAVE_BDB netcoind.exe >>"$LOG" 2>&1

log "building and running unit tests"
make -f makefile.mingw -j"$JOBS" USE_BDB=$HAVE_BDB test_netcoin.exe >>"$LOG" 2>&1
./test_netcoin.exe >>"$LOG" 2>&1

log "building netcoin.exe (Qt wallet)"
mkdir -p "$ROOT/build-qt"
cd "$ROOT/build-qt"
QRARG="USE_QRCODE=$HAVE_QR"
BDBARG=""
[ "$HAVE_BDB" = 1 ] || BDBARG="USE_BDB=0"
qmake ../netcoin-qt.pro MSYS2=1 USE_UPNP=1 $QRARG $BDBARG CONFIG+=release >>"$LOG" 2>&1
make -j"$JOBS" >>"$LOG" 2>&1

log "collecting files"
DIST="$ROOT/dist/netcoin-$VERSION-win64"
rm -rf "$DIST"
mkdir -p "$DIST"
cp "$ROOT/src/netcoind.exe" "$DIST/"
QTEXE="$(ls release/netcoin.exe 2>/dev/null || ls netcoin.exe)"
cp "$QTEXE" "$DIST/netcoin.exe"
strip "$DIST/netcoind.exe" "$DIST/netcoin.exe"
windeployqt --release --no-translations --no-system-d3d-compiler --no-opengl-sw \
    --no-compiler-runtime "$DIST/netcoin.exe" >>"$LOG" 2>&1 || true
# the Qt platform plugin is required for the wallet window to open
if [ ! -f "$DIST/platforms/qwindows.dll" ]; then
    for d in "$MINGW_PREFIX/share/qt5/plugins" "$MINGW_PREFIX/lib/qt5/plugins"; do
        if [ -f "$d/platforms/qwindows.dll" ]; then
            mkdir -p "$DIST/platforms" "$DIST/styles"
            cp "$d/platforms/qwindows.dll" "$DIST/platforms/"
            cp "$d"/styles/*.dll "$DIST/styles/" 2>/dev/null || true
            break
        fi
    done
fi
[ -f "$DIST/platforms/qwindows.dll" ] || { echo "qwindows.dll not found" >>"$LOG"; false; }
# all DLLs from the MSYS2 prefix that the programs (and Qt plugins) need
for i in 1 2 3; do
    find "$DIST" -name '*.exe' -o -name '*.dll' | while read -r f; do
        ldd "$f" 2>/dev/null | awk '{print $3}' | grep -i "^${MINGW_PREFIX:-/ucrt64}/" || true
    done | sort -u | while read -r dll; do
        [ -f "$DIST/$(basename "$dll")" ] || cp "$dll" "$DIST/"
    done
done
cp "$ROOT/COPYING" "$DIST/COPYING.txt"
cp "$ROOT/contrib/ci/README-windows.txt" "$DIST/README.txt"
cd "$ROOT/dist"
rm -f "netcoin-$VERSION-win64.zip"
if command -v zip >/dev/null; then
    zip -qr "netcoin-$VERSION-win64.zip" "netcoin-$VERSION-win64"
else
    powershell -NoProfile -Command "Compress-Archive -Path 'netcoin-$VERSION-win64' -DestinationPath 'netcoin-$VERSION-win64.zip'"
fi
ls -la "$ROOT/dist" | tee -a "$LOG"
trap - ERR
