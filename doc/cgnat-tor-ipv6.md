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

Install Tor (`sudo apt install tor`) and add to `/etc/tor/torrc`:

```
HiddenServiceDir /var/lib/tor/netcoin/
HiddenServicePort 11310 127.0.0.1:11310
```

Restart Tor; your address is in `/var/lib/tor/netcoin/hostname`
(a 56-character v3 `.onion` name).

`netcoin.conf`:

```
proxy=127.0.0.1:9050   # all outgoing connections through Tor
listen=1               # listening is off by default when a proxy is set
bind=127.0.0.1         # only Tor connects to us
```

Other nodes connect to you with (they also need Tor and `proxy=`):

```
addnode=<your-56-character-address>.onion:11310
```

Notes and current limitations:

* Connecting to v3 onion addresses works by name through the SOCKS5
  proxy (tested). The P2P protocol of this version can only *gossip*
  IPv4/IPv6 and old (v2) onion addresses, so a v3 address is not spread
  automatically — share it with other node operators and use `addnode`.
  For the same reason `-externalip=<v3>.onion` is not accepted.
* Without `proxy=` (no Tor for outgoing traffic) you can still run the
  onion service for incoming connections and connect out normally;
  then only the onion address hides your IP for incoming traffic.
* Tor v2 onion addresses no longer exist on the Tor network.
