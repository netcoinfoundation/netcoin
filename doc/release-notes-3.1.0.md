# NetCoin 3.1.0

NetCoin 3.1.0 brings the 2019 code base (3.0.0) back to life on current
operating systems and libraries, fixes a number of bugs and moves the
wallet to SQLite.

**Consensus rules and the P2P protocol (version 70005) are unchanged.**
3.1.0 nodes and 3.0.0 nodes run on the same network. The new client
re-validated the complete main chain, from the genesis block up to block
3,357,862, and arrived at the same best block as the 3.0.0 client
(`e356e35cce22aee9dbb2b0041c9c93227e04bb9b376621079e5c35014d226533`).

## Upgrading — please read

* **Back up `wallet.dat` first.**
* On the first start an old Berkeley DB `wallet.dat` is converted to the
  new SQLite format. The original file is kept as
  `wallet.dat.bdb-<time>.bak`. **3.0.0 clients cannot open the converted
  wallet**; to go back, restore the `.bak` file.
* If the conversion fails, the original wallet is left unchanged and the
  client stops with an error. Keys can then be moved with `dumpwallet`
  (old client) and `importwallet` (new client).
* The data directory is the same as before (`%APPDATA%\NetCoin` on
  Windows, `~/.netcoin` on Linux). The block chain does not need to be
  downloaded again.

## Platforms and downloads

* Windows 10/11 64-bit: `netcoin-<version>-win64.zip` — graphical wallet
  `netcoin.exe` and command-line node `netcoind.exe`.
* Ubuntu 24.04 and 26.04 (x86_64): `netcoin-<version>-ubuntu-<release>-x86_64.tar.gz`.
* Builds from source on Ubuntu 24.04 / 26.04 with the standard
  distribution packages only (`doc/build-ubuntu-24.04.md`), and on
  Windows with MSYS2 (`doc/build-windows.md`).
* Verify downloads with `SHA256SUMS.txt`.

## Wallet

* The wallet is stored in SQLite instead of Berkeley DB (one
  self-contained file; same approach as Bitcoin Core since 0.21).
* After `encryptwallet` the old unencrypted keys are physically removed
  from the file (previously they could remain in free database pages).
* `backupwallet` makes a consistent copy even while the wallet is in use.
* `-salvagewallet` and automatic recovery work with the new format and
  keep the key pool, so a recovered encrypted wallet can start again.
* Berkeley DB is now only needed to convert old wallets and is optional
  when building from source (`USE_BDB=0`).

## Performance

* Initial sync and `-loadblock` imports are much faster: every block
  used to trigger a walk back to the last synchronized checkpoint, which
  on a fresh node is the genesis block (O(n²)). Importing the full chain
  now takes about one and a half hours on a 2-core machine instead of
  several days.

## Bug fixes

* Memory leak in signature creation and verification (one leaked
  EC key per operation).
* `-loadblock` import stopped at block 2,872,661: a valid block was
  rejected by the "duplicate proof-of-stake" flood protection and the
  rest of the chain stayed orphaned. Blocks from a local file are still
  fully validated, but no longer subject to this network-only check.
* `-loadblock` ended the program with `exit(0)` without flushing the
  databases; the node now continues normally after the import.
* Crash on shutdown when the RPC port could not be opened.
* `AreInputsStandard` stopped checking a transaction's inputs after the
  first non-standard P2SH input.
* NULL pointer dereferences in signature checks before the block index is
  loaded and in `GetNextTargetRequired`.
* Undefined static initialisation order in the testnet checkpoints.
* `CreateTransaction` called `GetReservedKey()` inside `assert()`, so the
  call was skipped in builds without assertions.
* Seed nodes given as `host:port` could never be resolved, and the port
  was ignored.
* `--help` was missing all RPC, block creation and SSL options.
* The documented default RPC port was wrong: it is 11211 (testnet
  22444), not 11311 / 21311.
* Two threads opening the block/transaction database at the same time:
  the old bundled LevelDB then opened it twice (risk of corruption),
  newer LevelDB versions refused and the node stopped.
* I2P addresses given as `.oc.b32.i2p` were stored with the Tor prefix.
* Wallet database cursors leaked on error paths.
* Several accesses to empty vectors and a negative array index in hex
  parsing.
* About 150 incorrect `printf` formats for 64-bit values.
* The Qt wallet was linked with an executable stack.

## Security and libraries

* OpenSSL 3 (the 1.0.x API used before has been unsupported since 2019).
* RPC over SSL (`-rpcssl`) accepts TLS 1.2 or newer only, with a modern
  default cipher list.
* Key generation checks the result of the random number generator.
* Boost 1.83 – 1.92, LevelDB 1.23 (system library by default), miniupnpc
  up to 2.3, Qt 5.15, C++17; works with GCC 13 – 16.
* Checked with AddressSanitizer, UndefinedBehaviorSanitizer and cppcheck.

## Network

* Nodes behind NAT or carrier-grade NAT (CGNAT) can now be reachable:
  with a local Tor daemon the node creates its own Tor v3 onion service
  (`-listenonion`, default on) and advertises it (`doc/cgnat-tor-ipv6.md`).
* BIP155 `addrv2` address messages: Tor v3 `.onion` addresses are relayed
  between 3.1.0 nodes and stored in `peers.dat`. Older nodes are
  unaffected — they ignore the new messages and keep receiving addresses
  in the old format, without `.onion` v3 entries.
* `externalip=`, `addnode=` and `connect=` accept v3 `.onion` addresses.
* UPnP: support for miniupnpc 2.2.8+; logs when the router itself is
  behind carrier-grade NAT (CGNAT), where port mapping cannot help.

## Development

* The unit tests compile again and all 75 pass.
* GitHub Actions build and test every change on Ubuntu 24.04, Ubuntu
  26.04 and Windows; release packages are built by the "Release" workflow,
  which only the repository owner can start.

## Known limitations

* The automatic wallet conversion was tested with wallets created by
  Berkeley DB 5.3 on Linux. If a wallet from the old Windows client
  (Berkeley DB 6.2) cannot be converted, use `dumpwallet` / `importwallet`.
* IPv6 connectivity is implemented as before but was not tested in this
  release.
* At the time of this release the network has very few nodes and no new
  blocks have been produced since 30 September 2026. Proof-of-stake blocks
  are only created while wallets holding coins are online, unlocked for
  staking and connected.
