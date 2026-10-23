# Running a public NetCoin node (Ubuntu server / VPS)

A node with a public IP address that accepts incoming connections helps the
whole network: other wallets — including ones behind NAT or CGNAT — can
connect to it, download the block chain from it and learn about other
nodes through it. These steps set one up on Ubuntu **24.04** or **26.04**
as a system service that starts automatically.

Run everything as a normal user with `sudo`. Choose your Ubuntu version and
the release to install first:

```bash
UBU=26.04          # or 24.04
VER=v3.1.0-rc2     # see https://github.com/pwarszawski/netcoin/releases
```

## 1. Install the binary

Runtime libraries:

```bash
sudo apt update
if [ "$UBU" = 26.04 ]; then
  sudo apt install -y libboost-filesystem1.90.0 libboost-program-options1.90.0 \
    libboost-thread1.90.0 libboost-chrono1.90.0 libminiupnpc21
else
  sudo apt install -y libboost-filesystem1.83.0 libboost-program-options1.83.0 \
    libboost-thread1.83.0 libboost-chrono1.83.0t64 libminiupnpc17
fi
sudo apt install -y libssl3t64 libsqlite3-0 libleveldb1d libdb5.3++t64 wget
```

Download, verify and install:

```bash
cd /tmp
PKG=netcoin-$VER-ubuntu-$UBU-x86_64
wget https://github.com/pwarszawski/netcoin/releases/download/$VER/$PKG.tar.gz
wget https://github.com/pwarszawski/netcoin/releases/download/$VER/SHA256SUMS.txt
sha256sum --ignore-missing -c SHA256SUMS.txt     # must print "OK"
tar xzf $PKG.tar.gz
sudo install -m 755 $PKG/netcoind /usr/local/bin/netcoind
netcoind -? | head -1                            # NetCoin version ...
```

(You can also build from source, see [build-ubuntu-24.04.md](build-ubuntu-24.04.md),
and install the resulting `src/netcoind` the same way.)

## 2. User and configuration

The node runs as its own system user; its home `/var/lib/netcoin` is the
data directory.

```bash
sudo useradd --system --create-home --home-dir /var/lib/netcoin --shell /usr/sbin/nologin netcoin
sudo tee /var/lib/netcoin/netcoin.conf >/dev/null <<EOC
rpcuser=netcoinrpc
rpcpassword=$(openssl rand -hex 32)
server=1
listen=1
port=11310
maxconnections=125
addnode=95.179.224.137
EOC
sudo chown netcoin:netcoin /var/lib/netcoin/netcoin.conf
sudo chmod 600 /var/lib/netcoin/netcoin.conf
```

The RPC password is generated randomly. RPC only listens on 127.0.0.1, it
is not reachable from outside.

## 3. System service

```bash
sudo wget -O /etc/systemd/system/netcoind.service \
  https://raw.githubusercontent.com/pwarszawski/netcoin/ubuntu-24.04-revival/contrib/init/netcoind.service
sudo systemctl daemon-reload
sudo systemctl enable --now netcoind
```

## 4. Open the P2P port

Only needed if the `ufw` firewall is active (check with `sudo ufw status`);
also open TCP 11310 in your provider's firewall, if it has one:

```bash
sudo ufw allow 11310/tcp
```

## 5. Check it

```bash
alias nc-cli='sudo -u netcoin netcoind -datadir=/var/lib/netcoin'
nc-cli getinfo
nc-cli getconnectioncount
nc-cli getblockcount
sudo tail -f /var/lib/netcoin/debug.log      # Ctrl+C to stop
```

The node now downloads the whole block chain from the network, which can
take several hours.

### Faster start: import an existing block file (optional)

If you have a synchronized wallet elsewhere, you can copy its block file
instead. Close that wallet first. On Windows the file is
`%APPDATA%\NetCoin\blk0001.dat` (PowerShell:
`scp "$env:APPDATA\NetCoin\blk0001.dat" user@SERVER:/tmp/`). Then on the
server:

```bash
sudo systemctl stop netcoind
sudo mv /tmp/blk0001.dat /var/lib/netcoin/import.dat
sudo chown netcoin:netcoin /var/lib/netcoin/import.dat
sudo -u netcoin netcoind -datadir=/var/lib/netcoin -loadblock=/var/lib/netcoin/import.dat
```

Every block is fully verified during the import. When `nc-cli getblockcount`
(in a second terminal) stops growing, stop the node with Ctrl+C, delete
`/var/lib/netcoin/import.dat` and start the service again:
`sudo systemctl start netcoind`.

## 6. Tor (optional)

With a public IP the node is reachable anyway. With Tor it also gets a
`.onion` address, which nodes that connect only through Tor can use (see
[cgnat-tor-ipv6.md](cgnat-tor-ipv6.md)):

```bash
sudo apt install -y tor
sudo sed -i 's/^#ControlPort 9051/ControlPort 9051/; s/^#CookieAuthentication 1/CookieAuthentication 1/' /etc/tor/torrc
echo "CookieAuthFileGroupReadable 1" | sudo tee -a /etc/tor/torrc
sudo usermod -aG debian-tor netcoin
sudo systemctl restart tor
sudo systemctl restart netcoind
sudo grep "tor:" /var/lib/netcoin/debug.log | tail -3   # ... reachable as <name>.onion:11310
```

## Updating

Download the new release as in step 1, then:

```bash
sudo systemctl stop netcoind
sudo install -m 755 $PKG/netcoind /usr/local/bin/netcoind
sudo systemctl start netcoind
```

## Notes

* The node creates a wallet in its data directory; do not keep coins on a
  server — use your own wallet at home for that.
* Wallets behind NAT can use your node permanently with
  `addnode=<server IP>` in their `netcoin.conf`.
* Logs: `/var/lib/netcoin/debug.log`; service status:
  `sudo systemctl status netcoind`.
