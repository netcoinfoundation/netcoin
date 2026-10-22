NetCoin for Windows (64-bit)
============================

netcoin.exe   - the graphical wallet
netcoind.exe  - the headless node / RPC server (command line)

Keep all files of this folder together; the .dll files are required.

Data directory: %APPDATA%\NetCoin (the same as the old 3.0.0 client).

Coming from the old 3.0.0 client
--------------------------------
Close the old client first. On the first start the old Berkeley DB
wallet.dat is converted to the new SQLite format; the original file is kept
as wallet.dat.bdb-<time>.bak. The old 3.0.0 client cannot open the
converted wallet, so keep the .bak file if you might go back.

Always keep a backup of your wallet (File > Backup Wallet).

Running the node from the command line
--------------------------------------
Create %APPDATA%\NetCoin\netcoin.conf with at least

    rpcuser=netcoinrpc
    rpcpassword=<a long random string>

then run   netcoind.exe -daemon   and e.g.   netcoind.exe getinfo

Default ports: P2P 11310, RPC 11211.
