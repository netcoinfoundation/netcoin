#!/usr/bin/env bash
# Builds netcoind and netcoin (Qt wallet) on Ubuntu or Fedora and packages
# them as dist/netcoin-<ver>-<distro>-x86_64.tar.gz. Used by the GitHub
# workflows. Set USE_BDB=0 to build without Berkeley DB.
set -euo pipefail

NCVER="${1:-dev}"
JOBS="$(nproc)"
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
DISTRO="$(. /etc/os-release && echo "$ID-$VERSION_ID")"
USE_BDB="${USE_BDB:-1}"
if command -v qmake-qt5 >/dev/null; then QMAKE=qmake-qt5; else QMAKE="qmake -qt=5"; fi
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
make -f makefile.unix -j"$JOBS" USE_BDB="$USE_BDB" netcoind test_netcoin
./test_netcoin

mkdir -p "$ROOT/build-qt"
cd "$ROOT/build-qt"
$QMAKE ../netcoin-qt.pro USE_UPNP=1 USE_QRCODE=1 USE_BDB="$USE_BDB" CONFIG+=release
make -j"$JOBS"

NAME="netcoin-$NCVER-$DISTRO-x86_64"
DIST="$ROOT/dist/$NAME"
rm -rf "$DIST"
mkdir -p "$DIST"
cp "$ROOT/src/netcoind" "$ROOT/build-qt/netcoin" "$DIST/"
strip "$DIST/netcoind" "$DIST/netcoin"
case "$DISTRO" in
    fedora-*) cp "$ROOT/contrib/ci/README-fedora.txt" "$DIST/README.txt" ;;
    *)        cp "$ROOT/contrib/ci/README-linux.txt" "$DIST/" ;;
esac
cp "$ROOT/COPYING" "$DIST/"
# record which packages provide the shared libraries (for the README)
for f in "$DIST/netcoind" "$DIST/netcoin"; do
    ldd "$f" | awk '/=>/{print $3}' | while read -r l; do
        if command -v dpkg >/dev/null; then
            dpkg -S "$l" 2>/dev/null || dpkg -S "/usr$l" 2>/dev/null || true
        else
            rpm -qf --qf '%{NAME}\n' "$l" 2>/dev/null || true
        fi
    done | cut -d: -f1
done | sort -u | grep -v -E '^(libc6|libgcc-s1|libstdc\+\+6|glibc|libgcc|libstdc\+\+)$' > "$DIST/runtime-packages.txt" || true
cd "$ROOT/dist"
tar -czf "$NAME.tar.gz" "$NAME"
ls -la "$ROOT/dist"
trap - ERR
