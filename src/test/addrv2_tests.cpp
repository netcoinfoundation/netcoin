// Tests for Tor v3 addresses and the BIP155 (addrv2) serialization
#include <boost/test/unit_test.hpp>

#include "netbase.h"
#include "protocol.h"
#include "serialize.h"
#include "net.h"
#include "util.h"

using namespace std;

BOOST_AUTO_TEST_SUITE(addrv2_tests)

// a valid v3 address (pubkey 32 x 0x01 would not be on the curve, but the
// checksum is all that matters for parsing); generated with the Tor spec
static const string strOnion = "pg6mmjiyjmcrsslvykfwnntlaru7p5svn6y2ymmju6nubxndf4pscryd.onion";

BOOST_AUTO_TEST_CASE(torv3_parse_and_format)
{
    CNetAddr addr;
    BOOST_CHECK(addr.SetSpecial(strOnion));
    BOOST_CHECK(addr.IsTorV3());
    BOOST_CHECK(addr.IsTor());
    BOOST_CHECK(addr.IsValid());
    BOOST_CHECK(addr.IsRoutable());
    BOOST_CHECK(addr.GetNetwork() == NET_TOR);
    BOOST_CHECK(!addr.IsAddrV1Compatible());
    BOOST_CHECK_EQUAL(addr.ToStringIP(), strOnion);

    // broken checksum / wrong version are rejected
    string bad = strOnion;
    bad[10] = (bad[10] == 'a') ? 'b' : 'a';
    CNetAddr addrBad;
    BOOST_CHECK(!addrBad.SetSpecial(bad));

    // CService parsing with port
    CService serv(strOnion + ":11310");
    BOOST_CHECK(serv.IsTorV3());
    BOOST_CHECK_EQUAL(serv.GetPort(), 11310);
    BOOST_CHECK_EQUAL(serv.ToStringIP(), strOnion);
}

template<typename T>
static T RoundTrip(const T& obj, int nVersion)
{
    CDataStream ss(SER_NETWORK, nVersion);
    ss << obj;
    T ret;
    ss >> ret;
    BOOST_CHECK(ss.empty());
    return ret;
}

BOOST_AUTO_TEST_CASE(addrv2_serialization)
{
    const int v2 = PROTOCOL_VERSION | ADDRV2_FORMAT;

    CService ipv4("1.2.3.4:11310");
    CService ipv6("[2001:4860::1]:11310");
    CService onion(strOnion + ":11310");

    BOOST_CHECK(RoundTrip(ipv4, v2) == ipv4);
    BOOST_CHECK(RoundTrip(ipv6, v2) == ipv6);
    BOOST_CHECK(RoundTrip(onion, v2) == onion);
    BOOST_CHECK_EQUAL(RoundTrip(onion, v2).ToStringIP(), strOnion);

    // old format still 16 bytes + port
    CDataStream ss(SER_NETWORK, PROTOCOL_VERSION);
    ss << ipv4;
    BOOST_CHECK_EQUAL(ss.size(), 18U);

    // BIP155 encoding of an IPv4 address: id 1, length 4, bytes, port
    CDataStream ss2(SER_NETWORK, v2);
    ss2 << ipv4;
    BOOST_CHECK_EQUAL(HexStr(ss2.begin(), ss2.end()), "0104010203042c2e");

    // CAddress in addrv2: time, services as compact size, address, port
    CAddress a(onion, NODE_NETWORK);
    a.nTime = 1700000000;
    vector<CAddress> v(1, a), w;
    CDataStream ss3(SER_NETWORK, PROTOCOL_VERSION);
    CAddrV2Vector wrapOut(v), wrapIn(w);
    ss3 << wrapOut;
    ss3 >> wrapIn;
    BOOST_CHECK_EQUAL(w.size(), 1U);
    BOOST_CHECK(w[0] == a);
    BOOST_CHECK_EQUAL(w[0].nTime, a.nTime);
    BOOST_CHECK_EQUAL(w[0].nServices, a.nServices);

    // unknown network ids are read but give an invalid address
    CDataStream ss4(SER_NETWORK, v2);
    ss4 << (unsigned char)99 << (unsigned char)3 << (unsigned char)1 << (unsigned char)2 << (unsigned char)3;
    CNetAddr unknown;
    ss4 >> unknown;
    BOOST_CHECK(!unknown.IsValid());

    // oversized addresses are rejected
    CDataStream ss5(SER_NETWORK, v2);
    ss5 << (unsigned char)2;
    WriteCompactSize(ss5, 1000);
    CNetAddr big;
    BOOST_CHECK_THROW(ss5 >> big, std::ios_base::failure);
}

BOOST_AUTO_TEST_SUITE_END()
