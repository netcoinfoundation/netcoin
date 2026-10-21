//
// Unit tests for block-chain checkpoints
//
#include <boost/assign/list_of.hpp> // for 'map_list_of()'
#include <boost/test/unit_test.hpp>
#include <boost/foreach.hpp>

#include "../checkpoints.h"
#include "../util.h"

using namespace std;

BOOST_AUTO_TEST_SUITE(Checkpoints_tests)

BOOST_AUTO_TEST_CASE(sanity)
{
    uint256 p1000 = uint256("0xfd51dbe8a3874a2ff717af63733c19c134f921ecfe0e9bfc74cf80e9ccf97b12");
    uint256 p10000 = uint256("0xd4702398032bc5b7b3c2e971de88eb6a5d91474350ccf9fdc276b606748eed40");
    BOOST_CHECK(Checkpoints::CheckHardened(1000, p1000));
    BOOST_CHECK(Checkpoints::CheckHardened(10000, p10000));

    // Wrong hashes at checkpoints should fail:
    BOOST_CHECK(!Checkpoints::CheckHardened(1000, p10000));
    BOOST_CHECK(!Checkpoints::CheckHardened(10000, p1000));

    // ... but any hash not at a checkpoint should succeed:
    BOOST_CHECK(Checkpoints::CheckHardened(1000+1, p10000));
    BOOST_CHECK(Checkpoints::CheckHardened(10000+1, p1000));

    BOOST_CHECK(Checkpoints::GetTotalBlocksEstimate() >= 100000);
}

BOOST_AUTO_TEST_SUITE_END()
