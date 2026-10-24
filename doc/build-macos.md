# Building NetCoin on macOS

Tested on macOS 14 (Sonoma) on Apple Silicon with Homebrew.

## 1. Install the tools and dependencies

Install the Xcode command line tools (`xcode-select --install`) and
[Homebrew](https://brew.sh), then:

```sh
brew install boost openssl@3 sqlite leveldb miniupnpc berkeley-db qt@5 qrencode
```

`berkeley-db` is only needed to convert wallet files of NetCoin 3.0.0 and
older; build with `USE_BDB=0` to leave it out.

## 2. Headless daemon (`netcoind`)

```sh
git clone -b ubuntu-24.04-revival https://github.com/pwarszawski/netcoin.git
cd netcoin/src
make -f makefile.osx -j"$(sysctl -n hw.ncpu)"
make -f makefile.osx test_netcoin && ./test_netcoin
```

## 3. Graphical wallet (`NetCoin.app`)

```sh
cd netcoin
mkdir build-qt && cd build-qt
"$(brew --prefix qt@5)/bin/qmake" ../netcoin-qt.pro USE_UPNP=1 USE_QRCODE=1 CONFIG+=release
make -j"$(sysctl -n hw.ncpu)"
open netcoin.app
```

The app built this way uses the Homebrew libraries. To make a
self-contained app that runs on other Macs (as in the release `.dmg`),
see `contrib/ci/build-macos.sh` (`macdeployqt` + ad-hoc signing).

## Running

Data directory: `~/Library/Application Support/NetCoin`. For `netcoind`
create `netcoin.conf` there:

```
rpcuser=netcoinrpc
rpcpassword=<a long random string>
server=1
```

The release builds are not signed with an Apple Developer ID; see
`contrib/ci/README-macos.txt` for how to open them the first time.
