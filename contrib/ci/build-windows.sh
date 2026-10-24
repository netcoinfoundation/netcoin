#!/usr/bin/env bash
# Builds netcoind.exe and netcoin.exe (Qt wallet) in an MSYS2 UCRT64 shell
# and collects everything needed to run them into dist/netcoin-<ver>-win64/.
# Used by the GitHub workflows; can also be run by hand in MSYS2.
# By default everything is linked statically (two self-contained .exe files);
# STATIC=0 builds against the DLLs and ships them next to the programs.
set -euo pipefail

VERSION="${1:-dev}"
JOBS="$(nproc)"
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
P="${MINGW_PACKAGE_PREFIX:-mingw-w64-ucrt-x86_64}"
STATIC="${STATIC:-1}"
LOG="$ROOT/build-windows.log"
: > "$LOG"

log() { echo "== $*" | tee -a "$LOG"; }

fail() {
    # make the end of the log visible as an annotation in the Actions UI
    if [ -n "${GITHUB_ACTIONS:-}" ]; then
        local msg
        msg="$( (grep -E 'error:|Error [0-9]|undefined reference|cannot find|No such file|multiple definition|\.dll => |still needs|^QT_INSTALL_(PREFIX|BINS|ARCHDATA|DATA)|^QMAKE_(MKSPECS|SPEC|XSPEC)' "$LOG" | grep -v 'target not found' | cut -c1-240 | tail -15; echo '--- last lines ---'; grep -v -E '^(g\+\+|gcc|[[:space:]]*[0-9]* \||[[:space:]]+\||\.\./src/.*(required from|note:|warning:))' "$LOG" | tail -15 | cut -c1-240) | sed 's/%/%25/g' | sed ':a;N;$!ba;s/\n/%0A/g')"
        echo "::error title=Windows build failed::${msg:0:60000}"
    fi
    exit 1
}
trap fail ERR

log "installing packages"
REQUIRED="toolchain boost openssl sqlite3 leveldb miniupnpc zlib qt6-base qt6-tools"
[ "$STATIC" = 1 ] && REQUIRED="$REQUIRED qt6-static"
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
log "Berkeley DB (wallet conversion): $HAVE_BDB, QR codes: $HAVE_QR, static: $STATIC"

# a static LevelDB needs the libraries it was built with
LEVELDB_EXTRA_LIBS=""
if [ "$STATIC" = 1 ]; then
    for l in snappy crc32c; do
        [ -f "$MINGW_PREFIX/lib/lib$l.a" ] && LEVELDB_EXTRA_LIBS="$LEVELDB_EXTRA_LIBS -l$l"
    done
    for l in boost_filesystem-mt boost_thread-mt ssl crypto sqlite3 leveldb miniupnpc z; do
        [ -f "$MINGW_PREFIX/lib/lib$l.a" ] || { echo "static library lib$l.a missing" | tee -a "$LOG"; false; }
    done
    log "extra libraries for LevelDB:${LEVELDB_EXTRA_LIBS:- none}"
    # Berkeley DB: libdb_cxx.a is only an import library; use the real
    # static archive
    BDB_LIB_SUFFIX=""
    if [ "$HAVE_BDB" = 1 ]; then
        BDB_LIB_SUFFIX="none"
        for suffix in "" "-6.2"; do
            a="$MINGW_PREFIX/lib/libdb_cxx$suffix.a"
            # import libraries define _head_<dll> symbols, static archives do not
            if [ -f "$a" ] && ! nm "$a" 2>/dev/null | grep -q "_head_"; then BDB_LIB_SUFFIX="$suffix"; break; fi
        done
        ls -la "$MINGW_PREFIX"/lib/libdb_cxx* >>"$LOG" 2>&1 || true
        [ "$BDB_LIB_SUFFIX" != none ] || { echo "no static Berkeley DB library" | tee -a "$LOG"; false; }
        log "Berkeley DB static library: libdb_cxx$BDB_LIB_SUFFIX.a"
    fi
    # MSYS2 ships libqrencode only as a DLL: build the static library
    if [ "$HAVE_QR" = 1 ] && [ ! -f "$MINGW_PREFIX/lib/libqrencode.a" ]; then
        log "building static libqrencode"
        pacman -S --noconfirm --needed "$P-cmake" >>"$LOG" 2>&1
        rm -rf "$ROOT/build-qrencode"
        git clone -q --depth 1 -b v4.1.1 https://github.com/fukuchi/libqrencode.git "$ROOT/build-qrencode" >>"$LOG" 2>&1
        (cd "$ROOT/build-qrencode" && cmake -G "MSYS Makefiles" -DBUILD_SHARED_LIBS=OFF -DWITH_TOOLS=OFF \
            -DCMAKE_POLICY_VERSION_MINIMUM=3.5 -DCMAKE_INSTALL_PREFIX="$MINGW_PREFIX" . && make -j"$JOBS" && make install) >>"$LOG" 2>&1
        [ -f "$MINGW_PREFIX/lib/libqrencode.a" ] || { echo "static libqrencode not built" | tee -a "$LOG"; false; }
    fi
fi
MAKEARGS=(USE_BDB=$HAVE_BDB STATIC=$STATIC "LEVELDB_EXTRA_LIBS=$LEVELDB_EXTRA_LIBS" "BDB_LIB_SUFFIX=${BDB_LIB_SUFFIX:-}")

log "building netcoind.exe"
cd "$ROOT/src"
make -f makefile.mingw -j"$JOBS" "${MAKEARGS[@]}" netcoind.exe >>"$LOG" 2>&1

log "building and running unit tests"
make -f makefile.mingw -j"$JOBS" "${MAKEARGS[@]}" test_netcoin.exe >>"$LOG" 2>&1
./test_netcoin.exe >>"$LOG" 2>&1

log "building netcoin.exe (Qt wallet)"
# MSYS2 names the Qt 6 tools without a suffix (or with -qt6)
findtool() {
    for t in "$@"; do
        for c in "$t" "$MINGW_PREFIX/bin/$t" "$MINGW_PREFIX/qt6/bin/$t" "$MINGW_PREFIX/lib/qt6/bin/$t" "$MINGW_PREFIX/share/qt6/bin/$t"; do
            if command -v "$c" >/dev/null 2>&1; then command -v "$c"; return 0; fi
        done
    done
    return 1
}
if [ "$STATIC" = 1 ]; then
    QMAKE="$MINGW_PREFIX/qt6-static/bin/qmake"
    [ -x "$QMAKE" ] || QMAKE="$(find "$MINGW_PREFIX/qt6-static" -name 'qmake*.exe' -print -quit)"
    # the static Qt's qmake does not find its mkspecs by itself
    SPECDIR="$(find "$MINGW_PREFIX/qt6-static" -type d -name win32-g++ -print -quit)"
    if [ -n "$SPECDIR" ]; then
        # its built-in paths point to the prefix, but the mkspecs live below
        # share/qt6: tell qmake where they are with a qt.conf
        ARCHDATA="$(dirname "$(dirname "$SPECDIR")")"
        QTCONF="$ROOT/build-qt-qt.conf"
        printf '[Paths]\nPrefix=%s\nArchData=%s\nData=%s\nHostData=%s\nHostPrefix=%s\n' \
            "$(cygpath -m "$MINGW_PREFIX/qt6-static")" "$(cygpath -m "$ARCHDATA")" "$(cygpath -m "$ARCHDATA")" \
            "$(cygpath -m "$ARCHDATA")" "$(cygpath -m "$MINGW_PREFIX/qt6-static")" > "$QTCONF"
        cat "$QTCONF" >>"$LOG"
        QMAKESPECARG="-qtconf $(cygpath -m "$QTCONF")"
    fi
else
    QMAKE="$(findtool qmake6 qmake-qt6 qmake)"
fi
[ -n "$QMAKE" ] && [ -x "$QMAKE" ] || { echo "qmake not found" >>"$LOG"; ls "$MINGW_PREFIX/bin" | grep -i -E "qmake|deployqt" >>"$LOG"; false; }
WINDEPLOYQT="$(findtool windeployqt6 windeployqt-qt6 windeployqt)" || WINDEPLOYQT=""
LRELEASE="$(findtool lrelease-qt6 lrelease)" || { echo "lrelease not found" >>"$LOG"; false; }
"$QMAKE" ${QMAKESPECARG:-} -query >>"$LOG" 2>&1 || true
log "qmake: $QMAKE ${QMAKESPECARG:-}  lrelease: $LRELEASE  windeployqt: ${WINDEPLOYQT:-none}"
mkdir -p "$ROOT/build-qt"
cd "$ROOT/build-qt"
QRARG="USE_QRCODE=$HAVE_QR"
BDBARG=""
[ "$HAVE_BDB" = 1 ] || BDBARG="USE_BDB=0"
"$QMAKE" ${QMAKESPECARG:-} ../netcoin-qt.pro MSYS2=1 STATIC=$STATIC "LEVELDB_EXTRA_LIBS=$LEVELDB_EXTRA_LIBS" "BDB_LIB_SUFFIX=${BDB_LIB_SUFFIX:-}" \
    "QMAKE_LRELEASE=$(cygpath -m "$LRELEASE")" USE_UPNP=1 $QRARG $BDBARG CONFIG+=release >>"$LOG" 2>&1
make -j"$JOBS" >>"$LOG" 2>&1

log "collecting files"
DIST="$ROOT/dist/netcoin-$VERSION-win64"
rm -rf "$DIST"
mkdir -p "$DIST"
cp "$ROOT/src/netcoind.exe" "$DIST/"
QTEXE="$(ls release/netcoin.exe 2>/dev/null || ls netcoin.exe)"
cp "$QTEXE" "$DIST/netcoin.exe"
strip "$DIST/netcoind.exe" "$DIST/netcoin.exe"
if [ "$STATIC" = 1 ]; then
    # self-contained: nothing from the MSYS2 prefix may be needed at run time
    for f in "$DIST"/*.exe; do
        if ldd "$f" | grep -i "${MINGW_PREFIX:-/ucrt64}/" | tee -a "$LOG" | grep -q .; then
            echo "$f still needs DLLs from MSYS2" | tee -a "$LOG"; false
        fi
    done
else
    [ -n "$WINDEPLOYQT" ] && "$WINDEPLOYQT" --release --no-translations --no-system-d3d-compiler --no-opengl-sw \
        --no-compiler-runtime "$DIST/netcoin.exe" >>"$LOG" 2>&1 || true
    # the Qt platform plugin is required for the wallet window to open
    if [ ! -f "$DIST/platforms/qwindows.dll" ]; then
        for d in "$MINGW_PREFIX/share/qt6/plugins" "$MINGW_PREFIX/lib/qt6/plugins"; do
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
fi
cp "$ROOT/COPYING" "$DIST/COPYING.txt"
cp "$ROOT/contrib/ci/README-windows.txt" "$DIST/README.txt"
cd "$ROOT/dist"
rm -f "netcoin-$VERSION-win64.zip"
if command -v zip >/dev/null; then
    zip -qr "netcoin-$VERSION-win64.zip" "netcoin-$VERSION-win64"
else
    powershell -NoProfile -Command "Compress-Archive -Path 'netcoin-$VERSION-win64' -DestinationPath 'netcoin-$VERSION-win64.zip'"
fi
ls -la "$DIST" "$ROOT/dist" | tee -a "$LOG"
("$DIST/netcoind.exe" -? > "$ROOT/dist/help.txt" || true); grep -q Usage "$ROOT/dist/help.txt"; head -3 "$ROOT/dist/help.txt" | tee -a "$LOG"; rm -f "$ROOT/dist/help.txt"
trap - ERR
