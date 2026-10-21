// Copyright (c) 2026 The NetCoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "db_bdb.h"

#include <cstdio>
#include <cstring>
#include <fstream>

#include <boost/filesystem.hpp>

#ifdef ENABLE_BDB_MIGRATION
#include <db_cxx.h>
#endif

namespace fs = boost::filesystem;

bool IsBerkeleyDBFile(const fs::path& path)
{
    boost::system::error_code ec;
    if (!fs::is_regular_file(path, ec) || fs::file_size(path, ec) < 4096)
        return false;

    std::ifstream file(path.string().c_str(), std::ios::binary);
    if (!file.good())
        return false;
    unsigned char data[16];
    file.read((char*)data, sizeof(data));
    if (!file.good())
        return false;

    // Berkeley DB btree magic 0x00053162 at offset 12, in either byte order
    static const unsigned char magicLE[4] = {0x62, 0x31, 0x05, 0x00};
    static const unsigned char magicBE[4] = {0x00, 0x05, 0x31, 0x62};
    return memcmp(data + 12, magicLE, 4) == 0 || memcmp(data + 12, magicBE, 4) == 0;
}

#ifndef ENABLE_BDB_MIGRATION

bool HaveBerkeleyDBSupport() { return false; }

bool ReadBerkeleyDBWallet(const fs::path& pathDataDir, const std::string& strFile,
                          std::vector<BerkeleyRecord>& vRecords, std::string& strErr)
{
    strErr = "this build has no Berkeley DB support (built with USE_BDB=0)";
    return false;
}

#else

bool HaveBerkeleyDBSupport() { return true; }

namespace {

// Opens the copied file in a private environment and reads every record.
bool ReadAll(const fs::path& pathEnv, const std::string& strFile, bool fRecover,
             std::vector<BerkeleyRecord>& vRecords, std::string& strErr)
{
    vRecords.clear();
    DbEnv env(DB_CXX_NO_EXCEPTIONS);
    env.set_lg_dir("database");
    env.set_cachesize(0, 8 * 1024 * 1024, 1);
    env.set_lk_max_locks(10000);
    env.set_lk_max_objects(10000);
    u_int32_t nFlags = DB_CREATE | DB_INIT_LOCK | DB_INIT_LOG | DB_INIT_MPOOL |
                       DB_INIT_TXN | DB_PRIVATE;
    if (fRecover)
        nFlags |= DB_RECOVER;
    int ret = env.open(pathEnv.string().c_str(), nFlags, 0600);
    if (ret != 0)
    {
        strErr = std::string("cannot open Berkeley DB environment: ") + DbEnv::strerror(ret);
        env.close(0);
        return false;
    }

    bool fOk = false;
    {
        Db db(&env, 0);
        ret = db.open(NULL, strFile.c_str(), "main", DB_BTREE, DB_RDONLY, 0);
        if (ret != 0)
            strErr = std::string("cannot open wallet database: ") + DbEnv::strerror(ret);
        else
        {
            Dbc* pcursor = NULL;
            if (db.cursor(NULL, &pcursor, 0) != 0 || pcursor == NULL)
                strErr = "cannot create a cursor";
            else
            {
                while (true)
                {
                    Dbt key, value;
                    key.set_flags(DB_DBT_MALLOC);
                    value.set_flags(DB_DBT_MALLOC);
                    ret = pcursor->get(&key, &value, DB_NEXT);
                    if (ret == DB_NOTFOUND)
                    {
                        fOk = true;
                        break;
                    }
                    if (ret != 0)
                    {
                        strErr = std::string("read error: ") + DbEnv::strerror(ret);
                        break;
                    }
                    const unsigned char* pk = (const unsigned char*)key.get_data();
                    const unsigned char* pv = (const unsigned char*)value.get_data();
                    vRecords.push_back(BerkeleyRecord(std::vector<unsigned char>(pk, pk + key.get_size()),
                                                      std::vector<unsigned char>(pv, pv + value.get_size())));
                    // wipe and free the copies (they may contain private keys)
                    memset(key.get_data(), 0, key.get_size());
                    memset(value.get_data(), 0, value.get_size());
                    free(key.get_data());
                    free(value.get_data());
                }
                pcursor->close();
            }
            db.close(0);
        }
    }
    env.close(0);
    return fOk;
}

void CopyIfExists(const fs::path& from, const fs::path& to)
{
    boost::system::error_code ec;
    if (fs::is_regular_file(from, ec))
        fs::copy_file(from, to, fs::copy_options::overwrite_existing);
}

} // anon namespace

bool ReadBerkeleyDBWallet(const fs::path& pathDataDir, const std::string& strFile,
                          std::vector<BerkeleyRecord>& vRecords, std::string& strErr)
{
    fs::path pathTmp = pathDataDir / "bdb-migration.tmp";
    boost::system::error_code ec;
    bool fOk = false;
    std::string strErrRecover;
    try {
        // 1st attempt: replay the old environment's log files, in case the
        // old client did not shut down cleanly
        fs::path pathOldLogs = pathDataDir / "database";
        if (fs::is_directory(pathOldLogs, ec))
        {
            fs::remove_all(pathTmp, ec);
            fs::create_directories(pathTmp / "database");
            fs::copy_file(pathDataDir / strFile, pathTmp / strFile);
            for (fs::directory_iterator it(pathOldLogs); it != fs::directory_iterator(); ++it)
                if (it->path().filename().string().compare(0, 4, "log.") == 0)
                    CopyIfExists(it->path(), pathTmp / "database" / it->path().filename());
            fOk = ReadAll(pathTmp, strFile, true, vRecords, strErrRecover);
        }
        // 2nd attempt: the wallet file on its own (normal after a clean
        // shutdown, and the only option if the logs are from another
        // Berkeley DB version)
        if (!fOk)
        {
            fs::remove_all(pathTmp, ec);
            fs::create_directories(pathTmp / "database");
            fs::copy_file(pathDataDir / strFile, pathTmp / strFile);
            fOk = ReadAll(pathTmp, strFile, false, vRecords, strErr);
            if (!fOk && !strErrRecover.empty())
                strErr += " (with log recovery: " + strErrRecover + ")";
        }
    }
    catch (const std::exception& e) {
        strErr = e.what();
        fOk = false;
    }
    // the temporary copy may contain unencrypted keys: overwrite before removing
    try {
        if (fs::exists(pathTmp / strFile, ec))
        {
            boost::uintmax_t nSize = fs::file_size(pathTmp / strFile, ec);
            std::fstream f((pathTmp / strFile).string().c_str(), std::ios::in | std::ios::out | std::ios::binary);
            std::vector<char> zeros(65536, 0);
            for (boost::uintmax_t n = 0; f.good() && n < nSize; n += zeros.size())
                f.write(&zeros[0], std::min<boost::uintmax_t>(zeros.size(), nSize - n));
        }
    } catch (...) {}
    fs::remove_all(pathTmp, ec);
    return fOk;
}

#endif // ENABLE_BDB_MIGRATION
