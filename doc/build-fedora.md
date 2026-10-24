# Building NetCoin on Fedora

Tested on Fedora 44 and 45 (x86_64) with the packages from the standard
Fedora repositories.

## 1. Install the dependencies

```sh
sudo dnf install git gcc-c++ make \
    boost-devel openssl-devel sqlite-devel leveldb-devel miniupnpc-devel zlib-devel
# optional, only to convert wallet files of NetCoin 3.0.0 and older:
sudo dnf install libdb-cxx-devel
# only for the graphical wallet:
sudo dnf install qt5-qtbase-devel qt5-linguist qrencode-devel
```

## 2. Headless daemon (`netcoind`)

```sh
git clone -b ubuntu-24.04-revival https://github.com/pwarszawski/netcoin.git
cd netcoin/src
make -f makefile.unix -j"$(nproc)"
```

Without `libdb-cxx-devel` build with `make -f makefile.unix USE_BDB=0`
(an old Berkeley DB `wallet.dat` can then not be converted automatically;
move the keys with `dumpwallet` / `importwallet` instead).

Unit tests:

```sh
make -f makefile.unix -j"$(nproc)" test_netcoin
./test_netcoin
```

## 3. Graphical wallet (`netcoin`)

```sh
cd netcoin
mkdir build-qt && cd build-qt
qmake-qt5 ../netcoin-qt.pro USE_UPNP=1 USE_QRCODE=1 CONFIG+=release
make -j"$(nproc)"
./netcoin
```

(add `USE_BDB=0` to the qmake line if `libdb-cxx-devel` is not installed)

## Running

Data directory: `~/.netcoin`. For `netcoind` create `~/.netcoin/netcoin.conf`:

```
rpcuser=netcoinrpc
rpcpassword=<a long random string>
server=1
```

The firewall (firewalld) blocks incoming connections by default. To let
other nodes connect to you:

```sh
sudo firewall-cmd --permanent --add-port=11310/tcp && sudo firewall-cmd --reload
```

See [build-ubuntu-24.04.md](build-ubuntu-24.04.md) for the wallet format
and moving a wallet from another computer, and
[run-a-node.md](run-a-node.md) for running a public node.
