#!/usr/bin/env bash
# Builds netcoind and netcoin (Qt wallet) on Ubuntu and packages them as
# dist/netcoin-<ver>-<distro>-x86_64.tar.gz. Used by the GitHub workflows.
set -euo pipefail

NCVER="${1:-dev}"
JOBS="$(nproc)"
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
DISTRO="ubuntu-$(. /etc/os-release && echo "$VERSION_ID")"
LOG="$ROOT/build-linux.log"
: > "$LOG"
fail() {
    if [ -n "${GITHUB_ACTIONS:-}" ]; then
        local msg
        msg="$( (grep -E 'error:|Error [0-9]|undefined reference|cannot find|No such file' "$LOG" | tail -15; echo '--- last lines ---'; tail -25 "$LOG") | sed 's/%/%25/g' | sed ':a;N;$!ba;s/\n/%0A/g')"
        echo "::error title=Linux build failed::${msg:0:60000}"
    fi
    exit 1
}
trap fail ERR
# show everything in the job log and keep a copy for the error summary
exec > >(tee -a "$LOG") 2>&1

cd "$ROOT/src"
make -f makefile.unix -j"$JOBS" netcoind test_netcoin
./test_netcoin

mkdir -p "$ROOT/build-qt"
cd "$ROOT/build-qt"
qmake -qt=5 ../netcoin-qt.pro USE_UPNP=1 USE_QRCODE=1 CONFIG+=release
make -j"$JOBS"

NAME="netcoin-$NCVER-$DISTRO-x86_64"
DIST="$ROOT/dist/$NAME"
rm -rf "$DIST"
mkdir -p "$DIST"
cp "$ROOT/src/netcoind" "$ROOT/build-qt/netcoin" "$DIST/"
strip "$DIST/netcoind" "$DIST/netcoin"
cp "$ROOT/COPYING" "$ROOT/contrib/ci/README-linux.txt" "$DIST/"
# record which packages provide the shared libraries (for the README)
for f in "$DIST/netcoind" "$DIST/netcoin"; do
    ldd "$f" | awk '/=>/{print $3}' | while read -r l; do
        dpkg -S "$l" 2>/dev/null || dpkg -S "/usr$l" 2>/dev/null || true
    done | cut -d: -f1
done | sort -u | grep -v -E '^(libc6|libgcc-s1|libstdc\+\+6)$' > "$DIST/runtime-packages.txt" || true
cd "$ROOT/dist"
tar -czf "$NAME.tar.gz" "$NAME"
ls -la "$ROOT/dist"
trap - ERR
