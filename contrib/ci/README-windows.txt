NetCoin for Windows (64-bit)
============================

netcoin.exe   - the graphical wallet
netcoind.exe  - the headless node / RPC server (command line)

Both programs are self-contained; no other files are needed. Put them in
any folder (not in the data directory below).

First start: "Windows protected your PC"
----------------------------------------
The program is not signed with a paid code-signing certificate, so
Microsoft Defender SmartScreen warns about it. Click "More info" and then
"Run anyway" (once). Check the download against SHA256SUMS.txt from the
release page first, e.g. in PowerShell:

    Get-FileHash .\netcoin-<version>-win64.zip

If the zip was downloaded with a browser, you can also right-click it ->
Properties -> tick "Unblock" before extracting.

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
