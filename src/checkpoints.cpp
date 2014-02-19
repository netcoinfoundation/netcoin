// Copyright (c) 2009-2012 The Bitcoin developers
// Copyright (c) 2011-2012 Litecoin Developers
// Copyright (c) 2013 NetCoin Developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <boost/assign/list_of.hpp> // for 'map_list_of()'
#include <boost/foreach.hpp>

#include "checkpoints.h"

#include "main.h"
#include "uint256.h"

namespace Checkpoints
{
    typedef std::map<int, uint256> MapCheckpoints;

    //
    // What makes a good checkpoint block?
    // + Is surrounded by blocks with reasonable timestamps
    //   (no blocks before with a timestamp after, none after with
    //    timestamp before)
    // + Contains no strange transactions
    //

	// no checkpoint now, can be added in later releases
    static MapCheckpoints mapCheckpoints =
            boost::assign::map_list_of
            (     0, uint256("0x38624e3834cfdc4410a5acbc32f750171aadad9620e6ba6d5c73201c16f7c8d1"))
            (   100, uint256("0x722978629f5714f55eab4f5e6f03bed10c6bf34d9e73f79566c3eb0b94887c42"))
            (  1000, uint256("0xfd51dbe8a3874a2ff717af63733c19c134f921ecfe0e9bfc74cf80e9ccf97b12"))
            ( 10000, uint256("0xd4702398032bc5b7b3c2e971de88eb6a5d91474350ccf9fdc276b606748eed40"))
            ( 50000, uint256("0xb105dfb1ea9a486e73b502ccf71aec00ff3b6aee994dadd4f7f35e15a21c94e8"))
            (100000, uint256("0x10e02fc59b6e9d3010f0a91da8890fc6bacb59cebb38bdaad8b7f84df68fa617"))
            (136700, uint256("0x56635b3326e9a29f5ad82884eb3cfb7412a7c0daf671174daea834a0a4e80cb6"))
            (175000, uint256("0x1ea14545346fa619ff26000f50eb53f76bfb4861066ef8753c584e752db5b59f"))
            (200000, uint256("0xfc6fbe48af986b7c5e560ecbf82b78ea73f568482dd806e29e7d1027a1905fd4"))
			;


    bool CheckBlock(int nHeight, const uint256& hash)
    {
        if (fTestNet) return true; // Testnet has no checkpoints

        MapCheckpoints::const_iterator i = mapCheckpoints.find(nHeight);
        if (i == mapCheckpoints.end()) return true;
        return hash == i->second;
    }

    int GetTotalBlocksEstimate()
    {
        if (fTestNet) return 0;
	
        return mapCheckpoints.rbegin()->first;
    }

    CBlockIndex* GetLastCheckpoint(const std::map<uint256, CBlockIndex*>& mapBlockIndex)
    {
        if (fTestNet) return NULL;

        BOOST_REVERSE_FOREACH(const MapCheckpoints::value_type& i, mapCheckpoints)
        {
            const uint256& hash = i.second;
            std::map<uint256, CBlockIndex*>::const_iterator t = mapBlockIndex.find(hash);
            if (t != mapBlockIndex.end())
                return t->second;
        }
        return NULL;
    }
}
