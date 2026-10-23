// Copyright (c) 2009-2012 The Bitcoin developers
// Copyright (c) 2011-2012 Litecoin Developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
#ifndef BITCOIN_NETBASE_H
#define BITCOIN_NETBASE_H
#include <stdint.h>
#include <string>
#include <vector>

#include "serialize.h"
#include "compat.h"

extern int nConnectTimeout;

#ifdef WIN32
// In MSVC, this is defined as a macro, undefine it to prevent a compile and link error
#undef SetPort
#endif

enum Network
{
    NET_UNROUTABLE,
    NET_IPV4,
    NET_IPV6,
    NET_TOR,
    NET_I2P,

    NET_MAX,
};

extern int nConnectTimeout;
extern bool fNameLookup;

/** Stream version flag: serialize addresses in the BIP155 (addrv2) format,
 *  which can carry Tor v3 onion addresses. Used for "addrv2" messages and
 *  for peers.dat. */
static const int ADDRV2_FORMAT = 0x20000000;

/** BIP155 network IDs */
enum BIP155Network
{
    BIP155_IPV4 = 1,
    BIP155_IPV6 = 2,
    BIP155_TORV2 = 3,
    BIP155_TORV3 = 4,
    BIP155_I2P = 5,
    BIP155_CJDNS = 6,
};
static const unsigned int MAX_ADDRV2_SIZE = 512;
static const unsigned int TORV3_PUBKEY_SIZE = 32;

/** IP address (IPv6, or IPv4 using mapped IPv6 range (::FFFF:0:0/96)) */
class CNetAddr
{
    protected:
        unsigned char ip[16]; // in network byte order
        // Tor v3: the 32-byte ed25519 public key. ip[] then holds an internal
        // marker prefix and a hash of the key (used for hashing/grouping,
        // and unroutable for old nodes if it ever leaks into the old format).
        unsigned char torv3[32];

    public:
        CNetAddr();
        CNetAddr(const struct in_addr& ipv4Addr);
        explicit CNetAddr(const char *pszIp, bool fAllowLookup = false);
        explicit CNetAddr(const std::string &strIp, bool fAllowLookup = false);
        void Init();
        void SetIP(const CNetAddr& ip);
        bool SetSpecial(const std::string &strName); // for Tor and I2P addresses
        void SetTorV3(const unsigned char* pubkey);
        bool IsTorV3() const;
        /** Can be represented in the old 16-byte address format */
        bool IsAddrV1Compatible() const { return !IsTorV3(); }
        bool IsIPv4() const;    // IPv4 mapped address (::FFFF:0:0/96, 0.0.0.0/0)
        bool IsIPv6() const;    // IPv6 address (not mapped IPv4, not Tor/I2P)
        bool IsRFC1918() const; // IPv4 private networks (10.0.0.0/8, 192.168.0.0/16, 172.16.0.0/12)
        bool IsRFC3849() const; // IPv6 documentation address (2001:0DB8::/32)
        bool IsRFC3927() const; // IPv4 autoconfig (169.254.0.0/16)
        bool IsRFC3964() const; // IPv6 6to4 tunnelling (2002::/16)
        bool IsRFC4193() const; // IPv6 unique local (FC00::/15)
        bool IsRFC4380() const; // IPv6 Teredo tunnelling (2001::/32)
        bool IsRFC4843() const; // IPv6 ORCHID (2001:10::/28)
        bool IsRFC4862() const; // IPv6 autoconfig (FE80::/64)
        bool IsRFC6052() const; // IPv6 well-known prefix (64:FF9B::/96)
        bool IsRFC6145() const; // IPv6 IPv4-translated address (::FFFF:0:0:0/96)
        bool IsTor() const;
        bool IsI2P() const;
        bool IsLocal() const;
        bool IsRoutable() const;
        bool IsValid() const;
        bool IsMulticast() const;
        enum Network GetNetwork() const;
        std::string ToString() const;
        std::string ToStringIP() const;
        unsigned int GetByte(int n) const;
        uint64_t GetHash() const;
        bool GetInAddr(struct in_addr* pipv4Addr) const;
        std::vector<unsigned char> GetGroup() const;
        int GetReachabilityFrom(const CNetAddr *paddrPartner = NULL) const;
        void print() const;

// #ifdef USE_IPV6
        CNetAddr(const struct in6_addr& pipv6Addr);
        bool GetIn6Addr(struct in6_addr* pipv6Addr) const;
// #endif

        friend bool operator==(const CNetAddr& a, const CNetAddr& b);
        friend bool operator!=(const CNetAddr& a, const CNetAddr& b);
        friend bool operator<(const CNetAddr& a, const CNetAddr& b);

        // Serialization: 16 bytes (old format), or BIP155 network id +
        // compact size + address bytes when nVersion has ADDRV2_FORMAT.
        void SerializeV2(std::vector<unsigned char>& vchOut, unsigned char& nNetId) const;
        void UnserializeV2(unsigned char nNetId, const std::vector<unsigned char>& vch);

        unsigned int GetSerializeSize(int nType, int nVersion) const
        {
            if (!(nVersion & ADDRV2_FORMAT))
                return 16;
            std::vector<unsigned char> v;
            unsigned char nId;
            SerializeV2(v, nId);
            return 1 + ::GetSizeOfCompactSize(v.size()) + v.size();
        }
        template<typename Stream>
        void Serialize(Stream& s, int nType, int nVersion) const
        {
            if (!(nVersion & ADDRV2_FORMAT)) {
                s.write((const char*)ip, 16);
                return;
            }
            std::vector<unsigned char> v;
            unsigned char nId;
            SerializeV2(v, nId);
            s.write((const char*)&nId, 1);
            WriteCompactSize(s, v.size());
            if (!v.empty())
                s.write((const char*)&v[0], v.size());
        }
        template<typename Stream>
        void Unserialize(Stream& s, int nType, int nVersion)
        {
            Init();
            if (!(nVersion & ADDRV2_FORMAT)) {
                s.read((char*)ip, 16);
                return;
            }
            unsigned char nId;
            s.read((char*)&nId, 1);
            uint64_t nSize = ReadCompactSize(s);
            if (nSize > MAX_ADDRV2_SIZE)
                throw std::ios_base::failure("address too long");
            std::vector<unsigned char> v(nSize);
            if (nSize)
                s.read((char*)&v[0], nSize);
            UnserializeV2(nId, v);
        }
};

/** A combination of a network address (CNetAddr) and a (TCP) port */
class CService : public CNetAddr
{
    protected:
        unsigned short port; // host order

    public:
        CService();
        CService(const CNetAddr& ip, unsigned short port);
        CService(const struct in_addr& ipv4Addr, unsigned short port);
        CService(const struct sockaddr_in& addr);
        explicit CService(const char *pszIpPort, int portDefault, bool fAllowLookup = false);
        explicit CService(const char *pszIpPort, bool fAllowLookup = false);
        explicit CService(const std::string& strIpPort, int portDefault, bool fAllowLookup = false);
        explicit CService(const std::string& strIpPort, bool fAllowLookup = false);
        void Init();
        void SetPort(unsigned short portIn);
        unsigned short GetPort() const;
        bool GetSockAddr(struct sockaddr* paddr, socklen_t *addrlen) const;
        bool SetSockAddr(const struct sockaddr* paddr);
        friend bool operator==(const CService& a, const CService& b);
        friend bool operator!=(const CService& a, const CService& b);
        friend bool operator<(const CService& a, const CService& b);
        std::vector<unsigned char> GetKey() const;
        std::string ToString() const;
        std::string ToStringPort() const;
        std::string ToStringIPPort() const;
        void print() const;

// #ifdef USE_IPV6
        CService(const struct in6_addr& ipv6Addr, unsigned short port);
        CService(const struct sockaddr_in6& addr);
// #endif

        IMPLEMENT_SERIALIZE
            (
             CService* pthis = const_cast<CService*>(this);
             READWRITE(*(CNetAddr*)pthis);
             unsigned short portN = htons(port);
             READWRITE(portN);
             if (fRead)
                 pthis->port = ntohs(portN);
            )
};

typedef std::pair<CService, int> proxyType;

enum Network ParseNetwork(std::string net);
void SplitHostPort(std::string in, int &portOut, std::string &hostOut);
bool SetProxy(enum Network net, CService addrProxy, int nSocksVersion = 5);
bool GetProxy(enum Network net, proxyType &proxyInfoOut);
bool IsProxy(const CNetAddr &addr);
bool SetNameProxy(CService addrProxy, int nSocksVersion = 5);
bool HaveNameProxy();
bool LookupHost(const char *pszName, std::vector<CNetAddr>& vIP, unsigned int nMaxSolutions = 0, bool fAllowLookup = true);
bool LookupHostNumeric(const char *pszName, std::vector<CNetAddr>& vIP, unsigned int nMaxSolutions = 0);
bool Lookup(const char *pszName, CService& addr, int portDefault = 0, bool fAllowLookup = true);
bool Lookup(const char *pszName, std::vector<CService>& vAddr, int portDefault = 0, bool fAllowLookup = true, unsigned int nMaxSolutions = 0);
bool LookupNumeric(const char *pszName, CService& addr, int portDefault = 0);
bool ConnectSocket(const CService &addr, SOCKET& hSocketRet, int nTimeout = nConnectTimeout);
bool ConnectSocketByName(CService &addr, SOCKET& hSocketRet, const char *pszDest, int portDefault = 0, int nTimeout = nConnectTimeout);

#endif
