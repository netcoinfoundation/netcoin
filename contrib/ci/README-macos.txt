NetCoin for macOS (Apple Silicon, macOS 14 Sonoma or newer)
===========================================================

Drag NetCoin.app to Applications.

The app is not signed with an Apple Developer ID, so macOS blocks it on
the first start ("cannot be opened because the developer cannot be
verified" / "Apple could not verify ..."). To allow it once:

  - macOS 15 and newer: try to open the app, then System Settings ->
    Privacy & Security -> "NetCoin was blocked ..." -> Open Anyway.
  - macOS 14: right-click (Control-click) NetCoin.app -> Open -> Open.

Or in Terminal:  xattr -dr com.apple.quarantine /Applications/NetCoin.app

The headless node is inside the app:

  /Applications/NetCoin.app/Contents/MacOS/netcoind

Data directory: ~/Library/Application Support/NetCoin
Default ports: P2P 11310, RPC 11211. For netcoind, create netcoin.conf in
the data directory with rpcuser= and rpcpassword= first.

Tor (for nodes behind NAT/CGNAT): run the Tor Browser and add
torcontrol=127.0.0.1:9151 to netcoin.conf, or use Options -> Network.
