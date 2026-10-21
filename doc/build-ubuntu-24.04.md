# Building NetCoin on Ubuntu 24.04 (and newer)

This branch builds with the libraries shipped in the standard Ubuntu
repositories — no hand-built dependencies are needed.

| Library       | Ubuntu 24.04 package        | Version |
|---------------|-----------------------------|---------|
| OpenSSL       | `libssl-dev`                | 3.0     |
| Boost         | `libboost-all-dev`          | 1.83    |
| SQLite        | `libsqlite3-dev`            | 3.45    |
| Berkeley DB   | `libdb5.3++-dev` (optional) | 5.3     |
| LevelDB       | `libleveldb-dev`            | 1.23    |
| miniupnpc     | `libminiupnpc-dev`          | 2.2     |
| Qt (GUI only) | `qtbase5-dev`               | 5.15    |
| libqrencode   | `libqrencode-dev` (GUI)     | 4.1     |

## 1. Install the dependencies

```sh
sudo apt update
sudo apt install build-essential pkg-config git \
    libssl-dev libboost-all-dev libsqlite3-dev libdb5.3++-dev libleveldb-dev libminiupnpc-dev
# only for the graphical wallet:
sudo apt install qtbase5-dev qttools5-dev-tools libqrencode-dev
```

## 2. Headless daemon (`netcoind`)

```sh
git clone -b ubuntu-24.04-revival https://github.com/pwarszawski/netcoin.git
cd netcoin/src
make -f makefile.unix -j"$(nproc)"
strip netcoind          # optional, makes the binary much smaller
```

Useful make options:

* `USE_UPNP=-` – build without UPnP (default: compiled in, off at runtime;
  enable with `-upnp=1`)
* `USE_SYSTEM_LEVELDB=0` – use the bundled LevelDB 1.17 instead of the
  system library
* `USE_BDB=0` – build without Berkeley DB (then old wallet files cannot be
  converted automatically, see below)

Unit tests:

```sh
make -f makefile.unix -j"$(nproc)" test_netcoin
./test_netcoin
```

## 3. Graphical wallet (`netcoin`)

```sh
cd netcoin
mkdir build-qt && cd build-qt
qmake -qt=5 ../netcoin-qt.pro USE_UPNP=1 USE_QRCODE=1 CONFIG+=release
make -j"$(nproc)"
```

## 4. Configuration

Create `~/.netcoin/netcoin.conf` (the daemon refuses to start the RPC
server without a password):

```
rpcuser=netcoinrpc
rpcpassword=<a long random string>
server=1
```

Then `./netcoind -daemon` and e.g. `./netcoind getinfo`.

Default ports: P2P 11310, RPC 11211 (testnet: 21310 / 22444).

## Wallet format: SQLite

The wallet is now stored in SQLite (one self-contained `wallet.dat` file),
like Bitcoin Core does since 0.21, instead of Berkeley DB. The records
inside are unchanged.

When the client finds an old Berkeley DB `wallet.dat` in the data
directory it converts it once on startup:

* the original file is renamed to `wallet.dat.bdb-<time>.bak` and is not
  modified,
* every record is copied and read back for verification; if anything goes
  wrong the original is put back and the client stops with an error,
* the old `database/` log directory is no longer used and can be deleted
  after a successful conversion.

Note: the old (3.0.0) clients cannot open the converted file — keep the
`.bak` file if you may want to go back.

## Moving an existing wallet from the Windows client

Copy `wallet.dat` (with the old client **closed**) into the data directory
of the new client; it is converted automatically (see above).

If the conversion fails — the official Windows/macOS 3.0.0 binaries used
Berkeley DB 6.2 and its files may not be readable by 5.3 — use the
format-independent way:

1. In the old client (Debug console or `netcoind`):
   `walletpassphrase "<passphrase>" 300` (if encrypted), then
   `dumpwallet /path/to/netcoin-keys.txt`
2. Copy that file to the Linux machine over a secure channel.
3. In the new client: `importwallet /path/to/netcoin-keys.txt`
   (this rescans the block chain).
4. **Securely delete** the text file afterwards — it contains all private
   keys unencrypted. Re-encrypt the new wallet with `encryptwallet`.

Always keep a backup of the original `wallet.dat`.

The block chain does not need to be copied: either let the node sync, or
import the old `blk0001.dat` once with
`netcoind -loadblock=/path/to/blk0001.dat` (all blocks are re-verified).
