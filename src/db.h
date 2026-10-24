// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2012 The Bitcoin developers
// Copyright (c) 2011-2012 Litecoin Developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
#ifndef BITCOIN_DB_H
#define BITCOIN_DB_H

#include <stdint.h>

#include "sync.h"
#include "serialize.h"

#include <map>
#include <string>
#include <vector>

#include <boost/filesystem.hpp>
#include <boost/thread/recursive_mutex.hpp>

/*
 * Wallet key/value storage.
 *
 * Historically this was Berkeley DB. It is now a single SQLite file per
 * wallet (table "main", key BLOB PRIMARY KEY, value BLOB), with the same
 * serialized key/value records as before. Old Berkeley DB wallet files are
 * converted once at startup (see walletdb.cpp, MigrateBerkeleyWallet), the
 * original file is kept as a backup.
 *
 * The CDB interface (Read/Write/Erase/Exists/cursors/transactions) is kept
 * unchanged so CWalletDB did not need to change.
 */

struct sqlite3;
struct sqlite3_stmt;

class CAddrMan;
class CBlockLocator;
class CDiskBlockIndex;
class CDiskTxPos;
class CMasterKey;
class COutPoint;
class CTxIndex;
class CWallet;

extern unsigned int nWalletDBUpdated;

void ThreadFlushWalletDB(const std::string& strWalletFile);
bool BackupWallet(const CWallet& wallet, const std::string& strDest);

// Cursor flags / return codes (values kept from Berkeley DB for familiarity)
static const unsigned int DB_NEXT = 16;
static const unsigned int DB_SET_RANGE = 26;
static const int DB_NOTFOUND = -30988;

/** Sequential read access to a wallet database, in key order */
class CDBCursor
{
public:
    sqlite3* pdb;
    sqlite3_stmt* pstmt;
    bool fStarted;

    explicit CDBCursor(sqlite3* pdbIn) : pdb(pdbIn), pstmt(NULL), fStarted(false) {}
    ~CDBCursor();
    /** Releases the cursor (and deletes this object), like Dbc::close() */
    void close();
};
typedef CDBCursor Dbc;

/** One open SQLite database file, shared by all CDB objects using it */
struct CDBFile
{
    sqlite3* pdb;
    boost::recursive_mutex mutex; // serialises transactions on this file
    CDBFile() : pdb(NULL) {}
};

class CDBEnv
{
private:
    bool fDbEnvInit;
    bool fMockDb;
    boost::filesystem::path pathEnv;

public:
    mutable CCriticalSection cs_db;
    std::map<std::string, int> mapFileUseCount;
    std::map<std::string, CDBFile*> mapDb;

    CDBEnv();
    ~CDBEnv();
    void MakeMock();
    bool IsMock() { return fMockDb; }

    /*
     * Verify that database file strFile is OK. If it is not,
     * call the callback to try to recover.
     * This must be called BEFORE strFile is opened.
     * Returns true if strFile is OK.
     */
    enum VerifyResult { VERIFY_OK, RECOVER_OK, RECOVER_FAIL };
    VerifyResult Verify(std::string strFile, bool (*recoverFunc)(CDBEnv& dbenv, std::string strFile));
    /*
     * Read every record that can still be read from strFile.
     * Appends binary key/value pairs to vResult, returns true if no errors
     * were encountered.
     */
    typedef std::pair<std::vector<unsigned char>, std::vector<unsigned char> > KeyValPair;
    bool Salvage(std::string strFile, bool fAggressive, std::vector<KeyValPair>& vResult);

    bool Open(boost::filesystem::path pathEnv_);
    void Close();
    void Flush(bool fShutdown);
    void CheckpointLSN(const std::string& strFile);

    void CloseDb(const std::string& strFile);
    bool RemoveDb(const std::string& strFile);

    boost::filesystem::path GetPath(const std::string& strFile) const { return pathEnv / strFile; }

    /** Open (and create if fCreate) a SQLite wallet database file */
    static sqlite3* OpenFile(const boost::filesystem::path& path, bool fCreate, bool fReadOnly, std::string& strErr);
};

extern CDBEnv bitdb;


/** RAII class that provides access to a wallet database */
class CDB
{
protected:
    CDBFile* pfile;
    sqlite3* pdb;
    std::string strFile;
    bool activeTxn;
    bool fReadOnly;

    explicit CDB(const std::string& strFilename, const char* pszMode="r+");
    ~CDB() { Close(); }

public:
    void Close();

private:
    CDB(const CDB&);
    void operator=(const CDB&);

public:
    // raw (already serialized) record access, used by recovery/migration
    bool ReadRaw(const CDataStream& ssKey, CDataStream& ssValue);
    bool WriteRaw(const CDataStream& ssKey, const CDataStream& ssValue, bool fOverwrite);
    bool EraseRaw(const CDataStream& ssKey);
    bool ExistsRaw(const CDataStream& ssKey);

protected:

    template<typename K, typename T>
    bool Read(const K& key, T& value)
    {
        if (!pdb)
            return false;

        CDataStream ssKey(SER_DISK, CLIENT_VERSION);
        ssKey.reserve(1000);
        ssKey << key;

        CDataStream ssValue(SER_DISK, CLIENT_VERSION);
        bool fFound = ReadRaw(ssKey, ssValue);
        memset(&ssKey[0], 0, ssKey.size());
        if (!fFound)
            return false;

        bool fOk = true;
        try {
            ssValue >> value;
        }
        catch (std::exception &e) {
            fOk = false;
        }
        // clear memory in case it was a private key
        ssValue.clear();
        return fOk;
    }

    template<typename K, typename T>
    bool Write(const K& key, const T& value, bool fOverwrite=true)
    {
        if (!pdb)
            return false;
        if (fReadOnly)
            assert(!"Write called on database in read-only mode");

        CDataStream ssKey(SER_DISK, CLIENT_VERSION);
        ssKey.reserve(1000);
        ssKey << key;

        CDataStream ssValue(SER_DISK, CLIENT_VERSION);
        ssValue.reserve(10000);
        ssValue << value;

        bool fOk = WriteRaw(ssKey, ssValue, fOverwrite);

        // Clear memory in case it was a private key
        memset(&ssKey[0], 0, ssKey.size());
        if (ssValue.size())
            memset(&ssValue[0], 0, ssValue.size());
        return fOk;
    }

    template<typename K>
    bool Erase(const K& key)
    {
        if (!pdb)
            return false;
        if (fReadOnly)
            assert(!"Erase called on database in read-only mode");

        CDataStream ssKey(SER_DISK, CLIENT_VERSION);
        ssKey.reserve(1000);
        ssKey << key;
        bool fOk = EraseRaw(ssKey);
        memset(&ssKey[0], 0, ssKey.size());
        return fOk;
    }

    template<typename K>
    bool Exists(const K& key)
    {
        if (!pdb)
            return false;

        CDataStream ssKey(SER_DISK, CLIENT_VERSION);
        ssKey.reserve(1000);
        ssKey << key;
        bool fOk = ExistsRaw(ssKey);
        memset(&ssKey[0], 0, ssKey.size());
        return fOk;
    }

    Dbc* GetCursor();

    /** Read the next record. fFlags == DB_SET_RANGE (first call only)
     *  positions the cursor at the first key >= ssKey.
     *  Returns 0, DB_NOTFOUND at the end, or another non-zero value on error. */
    int ReadAtCursor(Dbc* pcursor, CDataStream& ssKey, CDataStream& ssValue, unsigned int fFlags=DB_NEXT);

public:
    bool TxnBegin();
    bool TxnCommit();
    bool TxnAbort();

    /** Consistent copy of this database (which may be in use) to pathDest */
    bool BackupTo(const boost::filesystem::path& pathDest);

    bool ReadVersion(int& nVersion)
    {
        nVersion = 0;
        return Read(std::string("version"), nVersion);
    }

    bool WriteVersion(int nVersion)
    {
        return Write(std::string("version"), nVersion);
    }

    /** Compact the file, optionally dropping all records whose key starts
     *  with pszSkip. Deleted data is overwritten (secure_delete), so this
     *  also scrubs e.g. unencrypted keys after the wallet is encrypted. */
    bool static Rewrite(const std::string& strFile, const char* pszSkip = NULL);
};




/** Access to the (IP) address database (peers.dat) */
class CAddrDB
{
private:
    boost::filesystem::path pathAddr;
public:
    CAddrDB();
    bool Write(const CAddrMan& addr);
    bool Read(CAddrMan& addr);
};

#endif // BITCOIN_DB_H
