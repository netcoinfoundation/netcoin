// Copyright (c) 2026 The NetCoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
#ifndef NETCOIN_QT_CONFFILE_H
#define NETCOIN_QT_CONFFILE_H

#include <map>
#include <string>
#include <vector>

/**
 * Small editor for netcoin.conf, used by the Options dialog. Comments,
 * unknown options and the order of the lines are kept.
 */
namespace ConfFile
{
    /** Values of all active (not commented out) "key=value" lines */
    std::vector<std::string> GetValues(const std::string& strKey);

    /**
     * Replaces all active lines of each given key by the given values (an
     * empty list removes the key). New lines are written where the key
     * first appeared, otherwise at the end of the file. Creates the file
     * if it does not exist.
     */
    bool SetValues(const std::map<std::string, std::vector<std::string> >& mapValues, std::string& strError);
}

#endif // NETCOIN_QT_CONFFILE_H
