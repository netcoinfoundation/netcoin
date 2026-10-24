// Copyright (c) 2026 The NetCoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
//
// Minimal Tor control protocol client (see Tor's control-spec.txt):
// PROTOCOLINFO, AUTHENTICATE (NULL / password / COOKIE / SAFECOOKIE),
// GETINFO net/listeners/socks and ADD_ONION ED25519-V3.

#include "torcontrol.h"

#include "net.h"
#include "netbase.h"
#include "util.h"

#include <boost/asio.hpp>
#include <boost/filesystem.hpp>
#include <boost/filesystem/fstream.hpp>
#include <boost/algorithm/string/predicate.hpp>
#include <boost/thread.hpp>

#include <openssl/hmac.h>
#include <openssl/rand.h>

#include <fstream>
#include <set>
#include <sstream>

namespace asio = boost::asio;

namespace {

class TorControlError : public std::runtime_error
{
public:
    explicit TorControlError(const std::string& msg) : std::runtime_error(msg) {}
};

/** Blocking line-based connection that stays responsive to thread interruption */
class TorConnection
{
    asio::io_context io;
    asio::ip::tcp::socket sock;
    std::string buffer;

public:
    TorConnection() : sock(io) {}

    void Connect(const std::string& strTarget)
    {
        int nPort = 9051;
        std::string strHost;
        SplitHostPort(strTarget, nPort, strHost);
        asio::ip::tcp::resolver resolver(io);
        asio::connect(sock, resolver.resolve(strHost, strprintf("%d", nPort)));
        sock.non_blocking(true);
    }

    void Send(const std::string& line)
    {
        std::string data = line + "\r\n";
        sock.non_blocking(false);
        asio::write(sock, asio::buffer(data));
        sock.non_blocking(true);
    }

    /** Next line without CRLF; throws if the connection is closed */
    std::string ReadLine()
    {
        while (true) {
            size_t pos = buffer.find("\r\n");
            if (pos != std::string::npos) {
                std::string line = buffer.substr(0, pos);
                buffer.erase(0, pos + 2);
                return line;
            }
            char chunk[1024];
            boost::system::error_code ec;
            size_t n = sock.read_some(asio::buffer(chunk), ec);
            if (ec == asio::error::would_block || ec == asio::error::try_again) {
                MilliSleep(200); // interruption point
                continue;
            }
            if (ec)
                throw TorControlError("connection closed: " + ec.message());
            buffer.append(chunk, n);
            if (buffer.size() > 100000)
                throw TorControlError("reply too long");
        }
    }

    /** Sends a command and returns all reply lines; throws on a non-250 reply */
    std::vector<std::string> Command(const std::string& line, const std::string& strLog = "")
    {
        if (fDebugNet)
            printf("tor: > %s\n", strLog.empty() ? line.c_str() : strLog.c_str());
        Send(line);
        std::vector<std::string> vReply;
        while (true) {
            std::string reply = ReadLine();
            if (reply.size() < 4)
                throw TorControlError("malformed reply");
            vReply.push_back(reply);
            if (reply[3] == ' ')
                break; // last line of the reply
        }
        if (vReply.back().compare(0, 3, "250") != 0)
            throw TorControlError(vReply.back());
        return vReply;
    }
};

/** Value of KEY=VALUE or KEY="VALUE" inside a reply line */
std::string GetReplyValue(const std::string& line, const std::string& key)
{
    size_t pos = line.find(key + "=");
    if (pos == std::string::npos)
        return "";
    pos += key.size() + 1;
    if (pos < line.size() && line[pos] == '"') {
        std::string val;
        for (pos++; pos < line.size() && line[pos] != '"'; pos++) {
            if (line[pos] == '\\' && pos + 1 < line.size())
                pos++;
            val += line[pos];
        }
        return val;
    }
    size_t end = line.find(' ', pos);
    return line.substr(pos, end == std::string::npos ? std::string::npos : end - pos);
}

std::vector<unsigned char> ReadCookie(const std::string& strPath)
{
    std::ifstream f(strPath.c_str(), std::ios::binary);
    if (!f.good())
        throw TorControlError("cannot read the Tor auth cookie " + strPath +
                              " (is this user allowed to read it?)");
    std::vector<unsigned char> v((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    if (v.size() != 32)
        throw TorControlError("unexpected Tor auth cookie size in " + strPath);
    return v;
}

std::vector<unsigned char> HmacSha256(const std::string& key, const std::vector<unsigned char>& data)
{
    std::vector<unsigned char> out(32);
    unsigned int nLen = 0;
    HMAC(EVP_sha256(), key.data(), key.size(), data.empty() ? NULL : &data[0], data.size(), &out[0], &nLen);
    return out;
}

void Authenticate(TorConnection& conn)
{
    std::vector<std::string> vInfo = conn.Command("PROTOCOLINFO 1");
    std::string strMethods, strCookieFile;
    BOOST_FOREACH(const std::string& line, vInfo) {
        if (line.compare(4, 4, "AUTH") == 0) {
            strMethods = GetReplyValue(line, "METHODS");
            strCookieFile = GetReplyValue(line, "COOKIEFILE");
        }
    }
    std::set<std::string> setMethods;
    std::stringstream ss(strMethods);
    std::string m;
    while (std::getline(ss, m, ','))
        setMethods.insert(m);

    if (mapArgs.count("-torpassword")) {
        std::string pw = mapArgs["-torpassword"], esc;
        BOOST_FOREACH(char c, pw) {
            if (c == '"' || c == '\\')
                esc += '\\';
            esc += c;
        }
        conn.Command("AUTHENTICATE \"" + esc + "\"", "AUTHENTICATE <password>");
    } else if (setMethods.count("NULL")) {
        conn.Command("AUTHENTICATE");
    } else if (setMethods.count("SAFECOOKIE")) {
        std::vector<unsigned char> cookie = ReadCookie(strCookieFile);
        std::vector<unsigned char> clientNonce(32);
        RAND_bytes(&clientNonce[0], clientNonce.size());
        std::vector<std::string> vChal = conn.Command("AUTHCHALLENGE SAFECOOKIE " + HexStr(clientNonce));
        std::vector<unsigned char> serverHash = ParseHex(GetReplyValue(vChal.back(), "SERVERHASH"));
        std::vector<unsigned char> serverNonce = ParseHex(GetReplyValue(vChal.back(), "SERVERNONCE"));
        std::vector<unsigned char> msg(cookie);
        msg.insert(msg.end(), clientNonce.begin(), clientNonce.end());
        msg.insert(msg.end(), serverNonce.begin(), serverNonce.end());
        if (HmacSha256("Tor safe cookie authentication server-to-controller hash", msg) != serverHash)
            throw TorControlError("SAFECOOKIE: Tor's reply did not verify");
        std::vector<unsigned char> clientHash = HmacSha256("Tor safe cookie authentication controller-to-server hash", msg);
        conn.Command("AUTHENTICATE " + HexStr(clientHash), "AUTHENTICATE <safecookie>");
    } else if (setMethods.count("COOKIE")) {
        conn.Command("AUTHENTICATE " + HexStr(ReadCookie(strCookieFile)), "AUTHENTICATE <cookie>");
    } else if (setMethods.count("HASHEDPASSWORD")) {
        throw TorControlError("Tor requires a password: set -torpassword");
    } else {
        throw TorControlError("no supported authentication method (" + strMethods + ")");
    }
}

/**
 * If no proxy for .onion is configured, use Tor's SOCKS port. Tor reports
 * it with GETINFO net/listeners/socks; when that gives nothing usable (e.g.
 * a unix socket, or an empty list) fall back to the usual port next to the
 * control port: 9150 for the Tor Browser (control 9151), otherwise 9050.
 */
void ConfigureOnionProxy(TorConnection& conn, const std::string& strControl)
{
    proxyType proxy;
    if (GetProxy(NET_TOR, proxy))
        return;

    CService addrProxy;
    try {
        std::vector<std::string> v = conn.Command("GETINFO net/listeners/socks");
        BOOST_FOREACH(const std::string& line, v) {
            if (line.find("net/listeners/socks=") == std::string::npos)
                continue;
            std::string val = line.substr(line.find('=') + 1);
            // space separated list of optionally quoted entries
            std::stringstream ss(val);
            std::string entry;
            while (ss >> entry) {
                if (entry.size() >= 2 && entry[0] == '"' && entry[entry.size() - 1] == '"')
                    entry = entry.substr(1, entry.size() - 2);
                CService addr(entry, 9050);
                if (addr.IsValid()) {
                    addrProxy = addr;
                    break;
                }
            }
            if (!addrProxy.IsValid())
                printf("tor: no usable SOCKS port in Tor's reply: %s\n", line.c_str());
        }
    } catch (const TorControlError& e) {
        printf("tor: cannot get the SOCKS port: %s\n", e.what());
    }

    if (!addrProxy.IsValid()) {
        CService addrControl(strControl, 9051);
        unsigned short nPort = (addrControl.GetPort() == 9151) ? 9150 : 9050;
        addrProxy = CService(addrControl.IsValid() ? addrControl.ToStringIP() : std::string("127.0.0.1"), nPort);
    }
    if (addrProxy.IsValid() && SetProxy(NET_TOR, addrProxy, 5)) {
        SetReachable(NET_TOR);
        printf("tor: using %s to connect to .onion peers\n", addrProxy.ToString().c_str());
    }
}

/** Creates the onion service; returns its address */
CService AddOnion(TorConnection& conn)
{
    boost::filesystem::path pathKey = GetDataDir() / "onion_v3_private_key";
    std::string strKey;
    {
        std::ifstream f(pathKey.string().c_str());
        std::getline(f, strKey);
    }
    unsigned short nPort = GetListenPort();
    std::string strTarget = strprintf("Port=%u,127.0.0.1:%u", nPort, nPort);
    std::vector<std::string> v;
    if (boost::starts_with(strKey, "ED25519-V3:"))
        v = conn.Command("ADD_ONION " + strKey + " " + strTarget, "ADD_ONION <saved key> " + strTarget);
    else
        v = conn.Command("ADD_ONION NEW:ED25519-V3 " + strTarget);

    std::string strServiceId;
    BOOST_FOREACH(const std::string& line, v) {
        std::string id = GetReplyValue(line, "ServiceID");
        if (!id.empty())
            strServiceId = id;
        std::string key = GetReplyValue(line, "PrivateKey");
        if (!key.empty()) {
            // keep the key so the node gets the same address next time
            std::ofstream f(pathKey.string().c_str(), std::ios::trunc);
            f << key << "\n";
            f.close();
#ifndef WIN32
            chmod(pathKey.string().c_str(), 0600);
#endif
        }
    }
    CService addr(strServiceId + ".onion", nPort);
    if (!addr.IsTorV3())
        throw TorControlError("unexpected ServiceID from Tor: " + strServiceId);
    return addr;
}

boost::mutex csOnionAddress;
std::string strOnionAddress;
bool fTorNetworkDisabled = false;

/** Tor Browser starts its Tor with the network disabled until "Connect" is
    clicked; an onion service created then is never published */
bool QueryNetworkDisabled(TorConnection& conn)
{
    std::vector<std::string> v = conn.Command("GETCONF DisableNetwork");
    for (const std::string& line : v)
        if (line.find("DisableNetwork=1") != std::string::npos)
            return true;
    return false;
}

void SetTorNetworkDisabled(bool fDisabled)
{
    {
        boost::lock_guard<boost::mutex> lock(csOnionAddress);
        if (fTorNetworkDisabled == fDisabled)
            return;
        fTorNetworkDisabled = fDisabled;
    }
    if (fDisabled)
        printf("tor: WARNING: Tor is running but not connected to the Tor network, so the .onion address cannot be reached (Tor Browser: click \"Connect\")\n");
    else
        printf("tor: Tor is connected to the Tor network\n");
}

void SetOnionServiceAddress(const std::string& str)
{
    boost::lock_guard<boost::mutex> lock(csOnionAddress);
    strOnionAddress = str;
}

} // anon namespace

std::string GetOnionServiceAddress()
{
    boost::lock_guard<boost::mutex> lock(csOnionAddress);
    return strOnionAddress;
}

bool IsTorNetworkDisabled()
{
    boost::lock_guard<boost::mutex> lock(csOnionAddress);
    return fTorNetworkDisabled;
}

void ThreadTorControl()
{
    RenameThread("netcoin-torcontrol");
    std::string strTarget = GetArg("-torcontrol", DEFAULT_TOR_CONTROL);
    bool fLoggedFailure = false;

    while (true) {
        CService addrOnion;
        try {
            TorConnection conn;
            conn.Connect(strTarget);
            Authenticate(conn);
            ConfigureOnionProxy(conn, strTarget);
            addrOnion = AddOnion(conn);
            printf("tor: this node is reachable as %s\n", addrOnion.ToString().c_str());
            AddLocal(addrOnion, LOCAL_MANUAL);
            SetOnionServiceAddress(addrOnion.ToString());
            fLoggedFailure = false;

            // The onion service exists as long as this control connection
            // is open. Check every 30 s that Tor is connected to the network;
            // a closed connection ends the loop with an exception.
            while (true) {
                SetTorNetworkDisabled(QueryNetworkDisabled(conn));
                MilliSleep(30 * 1000);
            }
        }
        catch (boost::thread_interrupted&) {
            throw;
        }
        catch (const std::exception& e) {
            if (!fLoggedFailure)
                printf("tor: cannot use the Tor control port %s: %s\n", strTarget.c_str(), e.what());
            fLoggedFailure = true; // do not fill the log while Tor is down
        }
        if (addrOnion.IsValid()) {
            RemoveLocal(addrOnion);
            SetOnionServiceAddress("");
        }
        SetTorNetworkDisabled(false);
        MilliSleep(60 * 1000);
    }
}
