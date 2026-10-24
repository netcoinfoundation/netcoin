NetCoin for Linux (x86_64)
==========================

netcoin   - the graphical wallet (Qt 6)
netcoind  - the headless node / RPC server

These binaries are linked against the libraries of the Ubuntu release named
in the archive name. Install the runtime libraries with:

  Ubuntu 24.04:
    sudo apt install libboost-filesystem1.83.0 libboost-program-options1.83.0 \
      libboost-thread1.83.0 libboost-chrono1.83.0t64 libssl3t64 libsqlite3-0 \
      libleveldb1d libminiupnpc17 libdb5.3++t64 libqt6widgets6t64 \
      qt6-qpa-plugins libqrencode4

  Ubuntu 26.04:
    sudo apt install libboost-filesystem1.90.0 libboost-program-options1.90.0 \
      libboost-thread1.90.0 libboost-chrono1.90.0 libssl3t64 libsqlite3-0 \
      libleveldb1d libminiupnpc21 libdb5.3++t64 libqt6widgets6t64 \
      qt6-qpa-plugins libqrencode4

The exact list for the archive is in runtime-packages.txt.
(or simply build from source, see doc/build-ubuntu-24.04.md).

Data directory: ~/.netcoin   Default ports: P2P 11310, RPC 11211.
Create ~/.netcoin/netcoin.conf with rpcuser= and rpcpassword= before
running netcoind.
