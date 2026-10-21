// Copyright (c) 2026 The NetCoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
#ifndef NETCOIN_DB_BDB_H
#define NETCOIN_DB_BDB_H

// Read-only access to legacy Berkeley DB wallet files, used once to convert
// them to the SQLite format. Deliberately independent of db.h: the Berkeley
// DB headers define macros (DB_NEXT, DB_NOTFOUND, ...) that clash with it.

#include <string>
#include <utility>
#include <vector>

#include <boost/filesystem/path.hpp>

typedef std::pair<std::vector<unsigned char>, std::vector<unsigned char> > BerkeleyRecord;

/** True if the file starts like a Berkeley DB btree database */
bool IsBerkeleyDBFile(const boost::filesystem::path& path);

/** True if this build can read Berkeley DB files */
bool HaveBerkeleyDBSupport();

/**
 * Read all records of the "main" database of a Berkeley DB wallet file.
 * Works on a temporary copy (plus the old "database" log directory, if
 * present, to replay a not cleanly closed wallet); the original files are
 * never modified.
 */
bool ReadBerkeleyDBWallet(const boost::filesystem::path& pathDataDir, const std::string& strFile,
                          std::vector<BerkeleyRecord>& vRecords, std::string& strErr);

#endif // NETCOIN_DB_BDB_H
