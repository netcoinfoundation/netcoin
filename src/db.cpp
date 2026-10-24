// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2012 The Bitcoin developers
// Copyright (c) 2011-2012 Litecoin Developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "chainparams.h"
#include "db.h"
#include "net.h"
#include "util.h"
#include "hash.h"
#include "addrman.h"

#include <boost/version.hpp>
#include <boost/filesystem.hpp>
#include <boost/filesystem/fstream.hpp>
#include <openssl/rand.h>

#include <sqlite3.h>

#ifndef WIN32
#include "sys/stat.h"
#endif

using namespace std;


unsigned int nWalletDBUpdated;

// "NETW" - lets tools recognise a NetCoin SQLite wallet
static const int WALLET_SQLITE_APPLICATION_ID = 0x4e455457;
static const int WALLET_SQLITE_SCHEMA_VERSION = 1;


//
// helpers
//

namespace {

bool ExecSQL(sqlite3* pdb, const char* pszSQL, std::string* pstrErr = NULL)
{
    char* pszErr = NULL;
    int ret = sqlite3_exec(pdb, pszSQL, NULL, NULL, &pszErr);
    if (ret != SQLITE_OK)
    {
        std::string strErr = pszErr ? pszErr : sqlite3_errstr(ret);
        printf("SQLite error in \"%s\": %s\n", pszSQL, strErr.c_str());
        if (pstrErr)
            *pstrErr = strErr;
        sqlite3_free(pszErr);
        return false;
    }
    return true;
}

// RAII prepared statement
class CStatement
{
public:
    sqlite3_stmt* pstmt;
    CStatement(sqlite3* pdb, const char* pszSQL) : pstmt(NULL)
    {
        if (sqlite3_prepare_v2(pdb, pszSQL, -1, &pstmt, NULL) != SQLITE_OK)
        {
            printf("SQLite prepare failed for \"%s\": %s\n", pszSQL, sqlite3_errmsg(pdb));
            pstmt = NULL;
        }
    }
    ~CStatement()
    {
        if (pstmt)
        {
            // do not leave copies of secrets in bound parameters
            sqlite3_clear_bindings(pstmt);
            sqlite3_finalize(pstmt);
        }
    }
    bool BindBlob(int n, const CDataStream& ss)
    {
        return sqlite3_bind_blob(pstmt, n, ss.size() ? &ss[0] : "", ss.size(), SQLITE_TRANSIENT) == SQLITE_OK;
    }
    bool BindBlob(int n, const std::vector<unsigned char>& v)
    {
        return sqlite3_bind_blob(pstmt, n, v.size() ? &v[0] : (const unsigned char*)"", v.size(), SQLITE_TRANSIENT) == SQLITE_OK;
    }
};

void ColumnToStream(sqlite3_stmt* pstmt, int nCol, CDataStream& ss)
{
    const char* p = (const char*)sqlite3_column_blob(pstmt, nCol);
    int n = sqlite3_column_bytes(pstmt, nCol);
    ss.SetType(SER_DISK);
    ss.clear();
    if (p && n > 0)
        ss.write(p, n);
}

} // anon namespace


//
// CDBCursor
//

CDBCursor::~CDBCursor()
{
    if (pstmt)
        sqlite3_finalize(pstmt);
}

void CDBCursor::close()
{
    delete this;
}


//
// CDBEnv
//

CDBEnv bitdb;

CDBEnv::CDBEnv()
{
    fDbEnvInit = false;
    fMockDb = false;
}

CDBEnv::~CDBEnv()
{
    Close();
}

void CDBEnv::Close()
{
    LOCK(cs_db);
    for (std::map<std::string, CDBFile*>::iterator it = mapDb.begin(); it != mapDb.end(); ++it)
    {
        if (it->second)
        {
            if (it->second->pdb)
                sqlite3_close_v2(it->second->pdb);
            delete it->second;
        }
    }
    mapDb.clear();
    fDbEnvInit = false;
}

bool CDBEnv::Open(boost::filesystem::path pathEnv_)
{
    if (fDbEnvInit)
        return true;

    boost::this_thread::interruption_point();

    pathEnv = pathEnv_;
    if (sqlite3_threadsafe() == 0)
        return error("CDBEnv::Open() : SQLite was built without thread safety");
    printf("Using SQLite %s for the wallet database\n", sqlite3_libversion());

    fDbEnvInit = true;
    fMockDb = false;
    return true;
}

void CDBEnv::MakeMock()
{
    if (fDbEnvInit)
        throw runtime_error("CDBEnv::MakeMock(): already initialized");

    boost::this_thread::interruption_point();
    printf("CDBEnv::MakeMock()\n");
    fDbEnvInit = true;
    fMockDb = true;
}

sqlite3* CDBEnv::OpenFile(const boost::filesystem::path& path, bool fCreate, bool fReadOnly, std::string& strErr)
{
    sqlite3* pdb = NULL;
    int nFlags = SQLITE_OPEN_FULLMUTEX;
    if (fReadOnly)
        nFlags |= SQLITE_OPEN_READONLY;
    else
        nFlags |= SQLITE_OPEN_READWRITE;
    if (fCreate && !fReadOnly)
        nFlags |= SQLITE_OPEN_CREATE;

    std::string strPath = path.string();
    int ret = sqlite3_open_v2(strPath.c_str(), &pdb, nFlags, NULL);
    if (ret != SQLITE_OK)
    {
        strErr = pdb ? sqlite3_errmsg(pdb) : sqlite3_errstr(ret);
        if (pdb)
            sqlite3_close_v2(pdb);
        return NULL;
    }
    sqlite3_extended_result_codes(pdb, 1);
    sqlite3_busy_timeout(pdb, 5000);

    // A file that is not a SQLite database (e.g. an old Berkeley DB wallet)
    // is only detected on first access:
    int nAppId = 0;
    {
        CStatement stmt(pdb, "PRAGMA application_id");
        if (!stmt.pstmt || sqlite3_step(stmt.pstmt) != SQLITE_ROW)
        {
            strErr = sqlite3_errmsg(pdb);
            sqlite3_close_v2(pdb);
            return NULL;
        }
        nAppId = sqlite3_column_int(stmt.pstmt, 0);
    }

    bool fOk = true;
    // overwrite deleted content (e.g. unencrypted keys) with zeros
    fOk &= ExecSQL(pdb, "PRAGMA secure_delete = ON");
    if (!fReadOnly)
    {
        // single file at rest, durable commits, no other process may use it
        fOk &= ExecSQL(pdb, "PRAGMA journal_mode = DELETE");
        fOk &= ExecSQL(pdb, "PRAGMA synchronous = FULL");
        fOk &= ExecSQL(pdb, "PRAGMA locking_mode = EXCLUSIVE");
        fOk &= ExecSQL(pdb, "PRAGMA foreign_keys = OFF");
        fOk &= ExecSQL(pdb, "CREATE TABLE IF NOT EXISTS main (key BLOB PRIMARY KEY NOT NULL, value BLOB NOT NULL)");
        if (nAppId == 0)
        {
            fOk &= ExecSQL(pdb, strprintf("PRAGMA application_id = %d", WALLET_SQLITE_APPLICATION_ID).c_str());
            fOk &= ExecSQL(pdb, strprintf("PRAGMA user_version = %d", WALLET_SQLITE_SCHEMA_VERSION).c_str());
            nAppId = WALLET_SQLITE_APPLICATION_ID;
        }
    }
    if (fOk && nAppId != WALLET_SQLITE_APPLICATION_ID)
    {
        strErr = "not a NetCoin wallet database";
        fOk = false;
    }
    if (!fOk)
    {
        if (strErr.empty())
            strErr = sqlite3_errmsg(pdb);
        sqlite3_close_v2(pdb);
        return NULL;
    }
    return pdb;
}

CDBEnv::VerifyResult CDBEnv::Verify(std::string strFile, bool (*recoverFunc)(CDBEnv& dbenv, std::string strFile))
{
    {
        LOCK(cs_db);
        assert(mapFileUseCount.count(strFile) == 0 || mapFileUseCount[strFile] == 0);
        // the shared connection holds an exclusive lock on the file
        CloseDb(strFile);

        bool fOk = false;
        std::string strErr;
        sqlite3* pdb = OpenFile(GetPath(strFile), false, true, strErr);
        if (pdb)
        {
            {
                CStatement stmt(pdb, "PRAGMA integrity_check");
                if (stmt.pstmt && sqlite3_step(stmt.pstmt) == SQLITE_ROW)
                {
                    const char* psz = (const char*)sqlite3_column_text(stmt.pstmt, 0);
                    fOk = (psz && strcmp(psz, "ok") == 0);
                    if (!fOk)
                        printf("Wallet %s integrity check: %s\n", strFile.c_str(), psz ? psz : "?");
                }
            }
            sqlite3_close_v2(pdb);
        }
        else
            printf("Wallet %s cannot be opened: %s\n", strFile.c_str(), strErr.c_str());

        if (fOk)
            return VERIFY_OK;
        if (recoverFunc == NULL)
            return RECOVER_FAIL;
    }

    // Try to recover:
    bool fRecovered = (*recoverFunc)(*this, strFile);
    return (fRecovered ? RECOVER_OK : RECOVER_FAIL);
}

bool CDBEnv::Salvage(std::string strFile, bool fAggressive,
                     std::vector<CDBEnv::KeyValPair >& vResult)
{
    LOCK(cs_db);
    std::string strErr;
    sqlite3* pdb = OpenFile(GetPath(strFile), false, true, strErr);
    if (!pdb)
    {
        printf("Salvage: cannot open %s: %s\n", strFile.c_str(), strErr.c_str());
        return false;
    }
    bool fAllOk = true;
    {
        CStatement stmt(pdb, "SELECT key, value FROM main");
        if (!stmt.pstmt)
            fAllOk = false;
        else
        {
            while (true)
            {
                int ret = sqlite3_step(stmt.pstmt);
                if (ret == SQLITE_DONE)
                    break;
                if (ret != SQLITE_ROW)
                {
                    printf("Salvage: read error in %s: %s\n", strFile.c_str(), sqlite3_errmsg(pdb));
                    fAllOk = false;
                    break; // a damaged b-tree page ends the scan
                }
                const unsigned char* pk = (const unsigned char*)sqlite3_column_blob(stmt.pstmt, 0);
                int nk = sqlite3_column_bytes(stmt.pstmt, 0);
                const unsigned char* pv = (const unsigned char*)sqlite3_column_blob(stmt.pstmt, 1);
                int nv = sqlite3_column_bytes(stmt.pstmt, 1);
                vResult.push_back(make_pair(std::vector<unsigned char>(pk, pk + nk),
                                            std::vector<unsigned char>(pv, pv + nv)));
            }
        }
    }
    sqlite3_close_v2(pdb);
    return fAllOk;
}


void CDBEnv::CheckpointLSN(const std::string& strFile)
{
    // nothing to do: SQLite with a rollback journal keeps the file
    // self-contained after every commit
}


CDB::CDB(const std::string& strFilename, const char* pszMode) :
    pfile(NULL), pdb(NULL), activeTxn(false)
{
    fReadOnly = (!strchr(pszMode, '+') && !strchr(pszMode, 'w'));
    if (strFilename.empty())
        return;

    bool fCreate = strchr(pszMode, 'c');

    {
        LOCK(bitdb.cs_db);
        if (!bitdb.Open(GetDataDir()))
            throw runtime_error("env open failed");

        strFile = strFilename;
        ++bitdb.mapFileUseCount[strFile];
        pfile = bitdb.mapDb[strFile];
        if (pfile == NULL)
        {
            std::string strErr;
            sqlite3* pdbNew = NULL;
            if (bitdb.IsMock())
            {
                sqlite3_open_v2(":memory:", &pdbNew, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_FULLMUTEX, NULL);
                if (pdbNew && !ExecSQL(pdbNew, "CREATE TABLE IF NOT EXISTS main (key BLOB PRIMARY KEY NOT NULL, value BLOB NOT NULL)"))
                {
                    sqlite3_close_v2(pdbNew);
                    pdbNew = NULL;
                }
            }
            else
                // the shared connection is always opened read-write; a
                // read-only CDB simply never writes
                pdbNew = CDBEnv::OpenFile(bitdb.GetPath(strFile), fCreate, false, strErr);

            if (pdbNew == NULL)
            {
                --bitdb.mapFileUseCount[strFile];
                std::string strFileErr = strFile;
                strFile = "";
                throw runtime_error(strprintf("CDB : can't open database %s: %s", strFileErr.c_str(), strErr.c_str()));
            }
            pfile = new CDBFile();
            pfile->pdb = pdbNew;
            bitdb.mapDb[strFile] = pfile;
        }
        pdb = pfile->pdb;
    }

    // (outside cs_db: lock order is file mutex -> cs_db, never the reverse)
    if (fCreate && !Exists(string("version")))
    {
        bool fTmp = fReadOnly;
        fReadOnly = false;
        WriteVersion(CLIENT_VERSION);
        fReadOnly = fTmp;
    }
}

void CDB::Close()
{
    if (!pdb)
        return;
    if (activeTxn)
        TxnAbort();
    pdb = NULL;
    pfile = NULL;

    {
        LOCK(bitdb.cs_db);
        --bitdb.mapFileUseCount[strFile];
    }
}

bool CDB::ReadRaw(const CDataStream& ssKey, CDataStream& ssValue)
{
    boost::recursive_mutex::scoped_lock lock(pfile->mutex);
    CStatement stmt(pdb, "SELECT value FROM main WHERE key = ?");
    if (!stmt.pstmt || !stmt.BindBlob(1, ssKey))
        return false;
    if (sqlite3_step(stmt.pstmt) != SQLITE_ROW)
        return false;
    ColumnToStream(stmt.pstmt, 0, ssValue);
    return true;
}

bool CDB::WriteRaw(const CDataStream& ssKey, const CDataStream& ssValue, bool fOverwrite)
{
    boost::recursive_mutex::scoped_lock lock(pfile->mutex);
    CStatement stmt(pdb, fOverwrite ? "INSERT OR REPLACE INTO main (key, value) VALUES (?, ?)"
                                    : "INSERT INTO main (key, value) VALUES (?, ?)");
    if (!stmt.pstmt || !stmt.BindBlob(1, ssKey) || !stmt.BindBlob(2, ssValue))
        return false;
    int ret = sqlite3_step(stmt.pstmt);
    if (ret != SQLITE_DONE)
    {
        if (!(ret == SQLITE_CONSTRAINT_PRIMARYKEY && !fOverwrite))
            printf("CDB::Write : %s\n", sqlite3_errmsg(pdb));
        return false;
    }
    return true;
}

bool CDB::EraseRaw(const CDataStream& ssKey)
{
    boost::recursive_mutex::scoped_lock lock(pfile->mutex);
    CStatement stmt(pdb, "DELETE FROM main WHERE key = ?");
    if (!stmt.pstmt || !stmt.BindBlob(1, ssKey))
        return false;
    // like Berkeley DB's DB_NOTFOUND, erasing a missing key is not an error
    return sqlite3_step(stmt.pstmt) == SQLITE_DONE;
}

bool CDB::ExistsRaw(const CDataStream& ssKey)
{
    boost::recursive_mutex::scoped_lock lock(pfile->mutex);
    CStatement stmt(pdb, "SELECT 1 FROM main WHERE key = ?");
    if (!stmt.pstmt || !stmt.BindBlob(1, ssKey))
        return false;
    return sqlite3_step(stmt.pstmt) == SQLITE_ROW;
}

Dbc* CDB::GetCursor()
{
    if (!pdb)
        return NULL;
    return new CDBCursor(pdb);
}

int CDB::ReadAtCursor(Dbc* pcursor, CDataStream& ssKey, CDataStream& ssValue, unsigned int fFlags)
{
    if (!pcursor)
        return 99999;
    if (!pcursor->fStarted)
    {
        pcursor->fStarted = true;
        bool fRange = (fFlags == DB_SET_RANGE);
        const char* pszSQL = fRange ? "SELECT key, value FROM main WHERE key >= ? ORDER BY key"
                                    : "SELECT key, value FROM main ORDER BY key";
        if (sqlite3_prepare_v2(pcursor->pdb, pszSQL, -1, &pcursor->pstmt, NULL) != SQLITE_OK)
        {
            printf("CDB::ReadAtCursor : %s\n", sqlite3_errmsg(pcursor->pdb));
            pcursor->pstmt = NULL;
            return 99999;
        }
        if (fRange &&
            sqlite3_bind_blob(pcursor->pstmt, 1, ssKey.size() ? &ssKey[0] : "", ssKey.size(), SQLITE_TRANSIENT) != SQLITE_OK)
            return 99999;
    }
    if (!pcursor->pstmt)
        return 99999;

    int ret = sqlite3_step(pcursor->pstmt);
    if (ret == SQLITE_DONE)
        return DB_NOTFOUND;
    if (ret != SQLITE_ROW)
    {
        printf("CDB::ReadAtCursor : %s\n", sqlite3_errmsg(pcursor->pdb));
        return 99999;
    }
    ColumnToStream(pcursor->pstmt, 0, ssKey);
    ColumnToStream(pcursor->pstmt, 1, ssValue);
    return 0;
}

bool CDB::TxnBegin()
{
    if (!pdb || activeTxn)
        return false;
    // held until TxnCommit/TxnAbort, so that writes of other threads to the
    // same file cannot end up inside (or be rolled back with) this transaction
    pfile->mutex.lock();
    if (!ExecSQL(pdb, "BEGIN IMMEDIATE"))
    {
        pfile->mutex.unlock();
        return false;
    }
    activeTxn = true;
    return true;
}

bool CDB::TxnCommit()
{
    if (!pdb || !activeTxn)
        return false;
    bool fOk = ExecSQL(pdb, "COMMIT");
    if (!fOk)
        ExecSQL(pdb, "ROLLBACK");
    activeTxn = false;
    pfile->mutex.unlock();
    return fOk;
}

bool CDB::TxnAbort()
{
    if (!pdb || !activeTxn)
        return false;
    bool fOk = ExecSQL(pdb, "ROLLBACK");
    activeTxn = false;
    pfile->mutex.unlock();
    return fOk;
}

void CDBEnv::CloseDb(const string& strFile)
{
    LOCK(cs_db);
    std::map<std::string, CDBFile*>::iterator it = mapDb.find(strFile);
    if (it != mapDb.end() && it->second != NULL)
    {
        if (it->second->pdb)
            sqlite3_close_v2(it->second->pdb);
        delete it->second;
        it->second = NULL;
    }
}

bool CDBEnv::RemoveDb(const string& strFile)
{
    this->CloseDb(strFile);

    LOCK(cs_db);
    boost::system::error_code ec;
    boost::filesystem::remove(GetPath(strFile), ec);
    return !ec;
}

bool CDB::BackupTo(const boost::filesystem::path& pathDest)
{
    if (!pdb)
        return false;

    // write to a temporary file first, then rename over the destination
    boost::filesystem::path pathTmp = pathDest;
    pathTmp += ".tmp";
    boost::system::error_code ec;
    boost::filesystem::remove(pathTmp, ec);

    bool fOk = false;
    {
        // no transaction of another thread may be in progress during the copy
        boost::recursive_mutex::scoped_lock lock(pfile->mutex);
        sqlite3* pdbDest = NULL;
        if (sqlite3_open_v2(pathTmp.string().c_str(), &pdbDest, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, NULL) == SQLITE_OK)
        {
            sqlite3_backup* pbackup = sqlite3_backup_init(pdbDest, "main", pdb, "main");
            if (pbackup)
            {
                int ret = sqlite3_backup_step(pbackup, -1);
                sqlite3_backup_finish(pbackup);
                fOk = (ret == SQLITE_DONE);
            }
            if (!fOk)
                printf("Backup : %s\n", sqlite3_errmsg(pdbDest));
        }
        if (pdbDest)
            sqlite3_close_v2(pdbDest);
    }

    if (fOk)
        fOk = RenameOver(pathTmp, pathDest);
    if (!fOk)
        boost::filesystem::remove(pathTmp, ec);
    return fOk;
}

bool CDB::Rewrite(const string& strFile, const char* pszSkip)
{
    while (true)
    {
        {
            LOCK(bitdb.cs_db);
            if (!bitdb.mapFileUseCount.count(strFile) || bitdb.mapFileUseCount[strFile] == 0)
            {
                bitdb.CloseDb(strFile);
                bitdb.mapFileUseCount.erase(strFile);

                printf("Rewriting %s...\n", strFile.c_str());
                std::string strErr;
                sqlite3* pdb = CDBEnv::OpenFile(bitdb.GetPath(strFile), false, false, strErr);
                if (!pdb)
                    return error("Rewriting of %s FAILED: %s", strFile.c_str(), strErr.c_str());

                bool fSuccess = ExecSQL(pdb, "BEGIN IMMEDIATE");
                if (fSuccess && pszSkip)
                {
                    std::vector<unsigned char> vchPrefix(pszSkip, pszSkip + strlen(pszSkip));
                    CStatement stmt(pdb, "DELETE FROM main WHERE substr(key, 1, ?) = ?");
                    fSuccess = stmt.pstmt &&
                               sqlite3_bind_int(stmt.pstmt, 1, vchPrefix.size()) == SQLITE_OK &&
                               stmt.BindBlob(2, vchPrefix) &&
                               sqlite3_step(stmt.pstmt) == SQLITE_DONE;
                }
                if (fSuccess)
                {
                    // Update version
                    CDataStream ssKey(SER_DISK, CLIENT_VERSION);
                    ssKey << string("version");
                    CDataStream ssValue(SER_DISK, CLIENT_VERSION);
                    ssValue << CLIENT_VERSION;
                    CStatement stmt(pdb, "UPDATE main SET value = ? WHERE key = ?");
                    fSuccess = stmt.pstmt && stmt.BindBlob(1, ssValue) && stmt.BindBlob(2, ssKey) &&
                               sqlite3_step(stmt.pstmt) == SQLITE_DONE;
                }
                fSuccess = fSuccess && ExecSQL(pdb, "COMMIT");
                if (!fSuccess)
                    ExecSQL(pdb, "ROLLBACK");
                // rebuild the file so no freed pages with old data remain
                fSuccess = fSuccess && ExecSQL(pdb, "VACUUM");
                sqlite3_close_v2(pdb);

                if (!fSuccess)
                    printf("Rewriting of %s FAILED!\n", strFile.c_str());
                return fSuccess;
            }
        }
        MilliSleep(100);
    }
    return false;
}


void CDBEnv::Flush(bool fShutdown)
{
    int64_t nStart = GetTimeMillis();
    // Close all files that are not in use
    printf("Flush(%s)%s\n", fShutdown ? "true" : "false", fDbEnvInit ? "" : " db not started");
    if (!fDbEnvInit)
        return;
    {
        LOCK(cs_db);
        map<string, int>::iterator mi = mapFileUseCount.begin();
        while (mi != mapFileUseCount.end())
        {
            string strFile = (*mi).first;
            int nRefCount = (*mi).second;
            printf("%s refcount=%d\n", strFile.c_str(), nRefCount);
            if (nRefCount == 0)
            {
                CloseDb(strFile);
                printf("%s closed\n", strFile.c_str());
                mapFileUseCount.erase(mi++);
            }
            else
                mi++;
        }
        printf("DBFlush(%s)%s ended %15" PRI64d "ms\n", fShutdown ? "true" : "false", fDbEnvInit ? "" : " db not started", GetTimeMillis() - nStart);
        if (fShutdown && mapFileUseCount.empty())
            Close();
    }
}


//
// CAddrDB
//

// unsigned char CAddrDB::pchMessageStart[4] = { 0x00, 0x00, 0x00, 0x00 };

CAddrDB::CAddrDB()
{
    pathAddr = GetDataDir() / "peers.dat";
}

bool CAddrDB::Write(const CAddrMan& addr)
{
    // Generate random temporary filename
    unsigned short randv = 0;
    RAND_bytes((unsigned char *)&randv, sizeof(randv));
    std::string tmpfn = strprintf("peers.dat.%04x", randv);

    // serialize addresses, checksum data up to that point, then append csum
    CDataStream ssPeers(SER_DISK, CLIENT_VERSION);
    // ssPeers << FLATDATA(pchMessageStart);
    // ssPeers << FLATDATA(CAddrDB::pchMessageStart);
    ssPeers << FLATDATA(Params().MessageStart());
    ssPeers << addr;
    uint256 hash = Hash(ssPeers.begin(), ssPeers.end());
    ssPeers << hash;

    // open temp output file, and associate with CAutoFile
    boost::filesystem::path pathTmp = GetDataDir() / tmpfn;
    FILE *file = fopen(pathTmp.string().c_str(), "wb");
    CAutoFile fileout = CAutoFile(file, SER_DISK, CLIENT_VERSION);
    if (!fileout)
        return error("CAddrman::Write() : open failed");

    // Write and commit header, data
    try {
        fileout << ssPeers;
    }
    catch (std::exception &e) {
        return error("CAddrman::Write() : I/O error");
    }
    FileCommit(fileout);
    fileout.fclose();

    // replace existing peers.dat, if any, with new peers.dat.XXXX
    if (!RenameOver(pathTmp, pathAddr))
        return error("CAddrman::Write() : Rename-into-place failed");

    return true;
}

bool CAddrDB::Read(CAddrMan& addr)
{
    // open input file, and associate with CAutoFile
    FILE *file = fopen(pathAddr.string().c_str(), "rb");
    CAutoFile filein = CAutoFile(file, SER_DISK, CLIENT_VERSION);
    if (!filein)
        return error("CAddrman::Read() : open failed");

    // use file size to size memory buffer
    // int fileSize = GetFilesize(filein);
    int fileSize = boost::filesystem::file_size(pathAddr);
    int dataSize = fileSize - sizeof(uint256);
    // Don't try to resize to a negative number if file is small
    if ( dataSize < 0 ) dataSize = 0;
    vector<unsigned char> vchData;
    vchData.resize(dataSize);
    uint256 hashIn;

    // read data and checksum from file
    try {
        filein.read((char *)&vchData[0], dataSize);
        filein >> hashIn;
    }
    catch (std::exception &e) {
        return error("CAddrman::Read() 2 : I/O error or stream data corrupted");
    }
    filein.fclose();

    CDataStream ssPeers(vchData, SER_DISK, CLIENT_VERSION);

    // verify stored checksum matches input data
    uint256 hashTmp = Hash(ssPeers.begin(), ssPeers.end());
    if (hashIn != hashTmp)
        return error("CAddrman::Read() : checksum mismatch; data corrupted");

    // de-serialize address data
    unsigned char pchMsgTmp[4];
    try {
        ssPeers >> FLATDATA(pchMsgTmp);
        ssPeers >> addr;
    }
    catch (std::exception &e) {
        return error("CAddrman::Read() : I/O error or stream data corrupted");
    }

    // finally, verify the network matches ours
    // if (memcmp(pchMsgTmp, CAddrDB::pchMessageStart, sizeof(pchMsgTmp)))
    if (memcmp(pchMsgTmp, Params().MessageStart(), sizeof(pchMsgTmp)))
        return error("CAddrman::Read() : invalid network magic number");

    return true;
}




