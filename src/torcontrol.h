// Copyright (c) 2026 The NetCoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
#ifndef NETCOIN_TORCONTROL_H
#define NETCOIN_TORCONTROL_H

#include <string>

static const char* const DEFAULT_TOR_CONTROL = "127.0.0.1:9051";
static const bool DEFAULT_LISTEN_ONION = true;

/**
 * Talks to a local Tor daemon over its control port, creates a Tor v3 onion
 * service that forwards to our P2P port and advertises the resulting
 * .onion address, so that the node is reachable even behind (CG)NAT.
 * Reconnects if Tor restarts. Runs until the thread is interrupted.
 */
void ThreadTorControl();

/** The node's current .onion address (host:port), or empty if none */
std::string GetOnionServiceAddress();

#endif // NETCOIN_TORCONTROL_H
