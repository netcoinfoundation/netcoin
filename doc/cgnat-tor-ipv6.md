# Running a NetCoin node behind CGNAT (Tor, IPv6)

Behind carrier-grade NAT (CGNAT) your node cannot accept incoming
connections, and UPnP does not help (the NAT is at your provider, not
your router). This does **not** stop you from using the wallet: outgoing
connections work normally, so the node syncs, sends transactions and
stakes. You only cannot *serve* other nodes.

Two ways to become reachable anyway:

## 1. IPv6

Many providers that use CGNAT for IPv4 give every customer public IPv6
addresses. NetCoin listens on IPv6 by default (dual stack), so if your
router does not block incoming IPv6 on port 11310, other IPv6 nodes can
connect to you. Check `getinfo` / `getpeerinfo` for IPv6 peers, and test
reachability of `[your-ipv6-address]:11310` from outside.

## 2. Tor onion service (works through any NAT)

Since 3.1.0 the node creates its own Tor v3 onion service automatically
when a Tor daemon with an enabled control port is running
(`-listenonion`, on by default). The `.onion` address is advertised to
other 3.1.0+ nodes in the new BIP155 (`addrv2`) address format, so the node
becomes reachable — and is found by others — even behind CGNAT. Nodes older
than 3.1.0 keep working with it but do not learn or relay `.onion`
addresses.

### Linux (Debian / Ubuntu)

```sh
sudo apt install tor
```

In `/etc/tor/torrc` enable the control port (both lines are usually
there, commented out):

```
ControlPort 9051
CookieAuthentication 1
CookieAuthFileGroupReadable 1
```

then `sudo systemctl restart tor` and allow your user to read Tor's auth
cookie: `sudo usermod -aG debian-tor $USER` (log in again afterwards).

### Windows

Run the Tor Browser (it provides a control port on 9151) or the Tor
Expert Bundle. In the Tor Browser click **Connect** and switch on
**Always connect automatically**: until it is connected, its Tor runs with
the network disabled, and the `.onion` address is created but cannot be
reached (the wallet then warns about it). Keep the Tor Browser open while
the wallet runs. Add to `netcoin.conf` (or set it in Options > Network):

```
torcontrol=127.0.0.1:9151
```

### Checking it

`debug.log` shows e.g.

```
tor: this node is reachable as <56 characters>.onion:11310
```

The onion key is kept in `onion_v3_private_key` in the data directory, so
the address stays the same across restarts (delete the file to get a new
address). Options: `-listenonion=0` disables this, `-torcontrol=<ip:port>`
sets the control port, `-torpassword=<pw>` uses password authentication
instead of the cookie. Outgoing connections to `.onion` peers use Tor's
SOCKS port automatically unless `-proxy` / `-tor` is set.

To send *all* traffic through Tor (hide your IP address) also set
`proxy=127.0.0.1:9050` and `listen=1`.

### Manual onion service (alternative)

Without a control port you can configure the service in `torrc` yourself:

```
HiddenServiceDir /var/lib/tor/netcoin/
HiddenServicePort 11310 127.0.0.1:11310
```

and announce the address from `/var/lib/tor/netcoin/hostname` with
`externalip=<address>.onion` in `netcoin.conf`.
