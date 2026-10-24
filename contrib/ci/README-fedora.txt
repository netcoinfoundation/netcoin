NetCoin for Fedora (x86_64)
===========================

netcoin   - the graphical wallet (Qt 5)
netcoind  - the headless node / RPC server

These binaries are linked against the libraries of the Fedora release named
in the archive name. Install the runtime libraries with:

  sudo dnf install boost-filesystem boost-program-options boost-thread \
    boost-chrono openssl-libs sqlite-libs leveldb miniupnpc libdb-cxx \
    qt5-qtbase-gui qrencode-libs

(libdb-cxx is only needed if runtime-packages.txt lists it; it is used to
convert wallet files of NetCoin 3.0.0 and older.)

The exact list for the archive is in runtime-packages.txt.
Or build from source, see doc/build-fedora.md.

Data directory: ~/.netcoin   Default ports: P2P 11310, RPC 11211.
Create ~/.netcoin/netcoin.conf with rpcuser= and rpcpassword= before
running netcoind.
