// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2012 The Bitcoin developers
// Copyright (c) 2011-2012 Litecoin Developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "walletdb.h"
#include "db_bdb.h"
#include "wallet.h"

// #include <boost/version.hpp>
#include <boost/filesystem.hpp>

using namespace std;
using namespace boost;

namespace {
/** Closes a cursor when leaving scope (a leaked cursor keeps the wallet file busy) */
struct CCursorGuard
{
    Dbc*& pcursor;
    explicit CCursorGuard(Dbc*& p) : pcursor(p) {}
    ~CCursorGuard() { if (pcursor) pcursor->close(); pcursor = NULL; }
};
}


static uint64_t nAccountingEntryNumber = 0;
extern bool fWalletUnlockStakingOnly;

//
// CWalletDB
//

bool CWalletDB::WriteName(const string& strAddress, const string& strName)
{
    nWalletDBUpdated++;
    return Write(make_pair(string("name"), strAddress), strName);
}

bool CWalletDB::EraseName(const string& strAddress)
{
    // This should only be used for sending addresses, never for receiving addresses,
    // receiving addresses must always have an address book entry if they're not change return.
    nWalletDBUpdated++;
    return Erase(make_pair(string("name"), strAddress));
}

bool CWalletDB::ReadAccount(const string& strAccount, CAccount& account)
{
    account.SetNull();
    return Read(make_pair(string("acc"), strAccount), account);
}

bool CWalletDB::WriteAccount(const string& strAccount, const CAccount& account)
{
    return Write(make_pair(string("acc"), strAccount), account);
}

bool CWalletDB::WriteAccountingEntry(const uint64_t nAccEntryNum, const CAccountingEntry& acentry)
{
    return Write(boost::make_tuple(string("acentry"), acentry.strAccount, nAccEntryNum), acentry);
}

bool CWalletDB::WriteAccountingEntry(const CAccountingEntry& acentry)
{
    return WriteAccountingEntry(++nAccountingEntryNumber, acentry);
}

int64_t CWalletDB::GetAccountCreditDebit(const string& strAccount)
{
    list<CAccountingEntry> entries;
    ListAccountCreditDebit(strAccount, entries);

    int64_t nCreditDebit = 0;
    BOOST_FOREACH (const CAccountingEntry& entry, entries)
        nCreditDebit += entry.nCreditDebit;

    return nCreditDebit;
}

void CWalletDB::ListAccountCreditDebit(const string& strAccount, list<CAccountingEntry>& entries)
{
    bool fAllAccounts = (strAccount == "*");

    Dbc* pcursor = GetCursor();
    if (!pcursor)
        throw runtime_error("CWalletDB::ListAccountCreditDebit() : cannot create DB cursor");
    CCursorGuard cursorGuard(pcursor); // closes it on every exit path
    unsigned int fFlags = DB_SET_RANGE;
    while (true)
    {
        // Read next record
        CDataStream ssKey(SER_DISK, CLIENT_VERSION);
        if (fFlags == DB_SET_RANGE)
            ssKey << boost::make_tuple(string("acentry"), (fAllAccounts? string("") : strAccount), uint64_t(0));
        CDataStream ssValue(SER_DISK, CLIENT_VERSION);
        int ret = ReadAtCursor(pcursor, ssKey, ssValue, fFlags);
        fFlags = DB_NEXT;
        if (ret == DB_NOTFOUND)
            break;
        else if (ret != 0)
        {
            pcursor->close(); pcursor = NULL;
            throw runtime_error("CWalletDB::ListAccountCreditDebit() : error scanning DB");
        }

        // Unserialize
        string strType;
        ssKey >> strType;
        if (strType != "acentry")
            break;
        CAccountingEntry acentry;
        ssKey >> acentry.strAccount;
        if (!fAllAccounts && acentry.strAccount != strAccount)
            break;

        ssValue >> acentry;
        // Added in NetcoinPOS
        ssKey >> acentry.nEntryNo;
        // End Added in NetcoinPOS
        entries.push_back(acentry);
    }

    pcursor->close(); pcursor = NULL;
}


DBErrors
CWalletDB::ReorderTransactions(CWallet* pwallet)
{
    LOCK(pwallet->cs_wallet);
    // Old wallets didn't have any defined order for transactions
    // Probably a bad idea to change the output of this

    // First: get all CWalletTx and CAccountingEntry into a sorted-by-time multimap.
    typedef pair<CWalletTx*, CAccountingEntry*> TxPair;
    typedef multimap<int64_t, TxPair > TxItems;
    TxItems txByTime;

    for (map<uint256, CWalletTx>::iterator it = pwallet->mapWallet.begin(); it != pwallet->mapWallet.end(); ++it)
    {
        CWalletTx* wtx = &((*it).second);
        txByTime.insert(make_pair(wtx->nTimeReceived, TxPair(wtx, (CAccountingEntry*)0)));
    }
    list<CAccountingEntry> acentries;
    ListAccountCreditDebit("", acentries);
    BOOST_FOREACH(CAccountingEntry& entry, acentries)
    {
        txByTime.insert(make_pair(entry.nTime, TxPair((CWalletTx*)0, &entry)));
    }

    int64_t& nOrderPosNext = pwallet->nOrderPosNext;
    nOrderPosNext = 0;
    std::vector<int64_t> nOrderPosOffsets;
    for (TxItems::iterator it = txByTime.begin(); it != txByTime.end(); ++it)
    {
        CWalletTx *const pwtx = (*it).second.first;
        CAccountingEntry *const pacentry = (*it).second.second;
        int64_t& nOrderPos = (pwtx != 0) ? pwtx->nOrderPos : pacentry->nOrderPos;

        if (nOrderPos == -1)
        {
            nOrderPos = nOrderPosNext++;
            nOrderPosOffsets.push_back(nOrderPos);

            //if (pacentry)
                // Have to write accounting regardless, since we don't keep it in memory
            if (pwtx)
            {
                if (!WriteTx(pwtx->GetHash(), *pwtx))
                    return DB_LOAD_FAIL;
            }
            else
            if (!WriteAccountingEntry(pacentry->nEntryNo, *pacentry))
                    return DB_LOAD_FAIL;
        }
        else
        {
            int64_t nOrderPosOff = 0;
            BOOST_FOREACH(const int64_t& nOffsetStart, nOrderPosOffsets)
            {
                if (nOrderPos >= nOffsetStart)
                    ++nOrderPosOff;
            }
            nOrderPos += nOrderPosOff;
            nOrderPosNext = std::max(nOrderPosNext, nOrderPos + 1);

            if (!nOrderPosOff)
                continue;

            // Since we're changing the order, write it back
            if (pwtx)
            {
                if (!WriteTx(pwtx->GetHash(), *pwtx))
                    return DB_LOAD_FAIL;
            }
            else
                if (!WriteAccountingEntry(pacentry->nEntryNo, *pacentry))
                    return DB_LOAD_FAIL;
        }
    }
    WriteOrderPosNext(nOrderPosNext);

    return DB_LOAD_OK;
}

class CWalletScanState {
public:
    unsigned int nKeys;
    unsigned int nCKeys;
    unsigned int nKeyMeta;
    bool fIsEncrypted;
    bool fAnyUnordered;
    int nFileVersion;
    vector<uint256> vWalletUpgrade;

    CWalletScanState() {
        nKeys = nCKeys = nKeyMeta = 0;
        fIsEncrypted = false;
        fAnyUnordered = false;
        nFileVersion = 0;
    }
};

bool
ReadKeyValue(CWallet* pwallet, CDataStream& ssKey, CDataStream& ssValue,
             CWalletScanState &wss, string& strType, string& strErr)
{
    try {
            // Unserialize
            // Taking advantage of the fact that pair serialization
            // is just the two items serialized one after the other
            ssKey >> strType;
            if (strType == "name")
            {
                string strAddress;
                ssKey >> strAddress;
                ssValue >> pwallet->mapAddressBook[CBitcoinAddress(strAddress).Get()];
            }
            else if (strType == "tx")
            {
                uint256 hash;
                ssKey >> hash;
                CWalletTx& wtx = pwallet->mapWallet[hash];
                ssValue >> wtx;
            if (wtx.CheckTransaction() && (wtx.GetHash() == hash))
                wtx.BindWallet(pwallet);
            else
            {
                pwallet->mapWallet.erase(hash);
                return false;
            }

                // Undo serialize changes in 31600
                if (31404 <= wtx.fTimeReceivedIsTxTime && wtx.fTimeReceivedIsTxTime <= 31703)
                {
                    if (!ssValue.empty())
                    {
                        char fTmp;
                        char fUnused;
                        ssValue >> fTmp >> fUnused >> wtx.strFromAccount;
                    strErr = strprintf("LoadWallet() upgrading tx ver=%d %d '%s' %s",
                                       wtx.fTimeReceivedIsTxTime, fTmp, wtx.strFromAccount.c_str(), hash.ToString().c_str());
                        wtx.fTimeReceivedIsTxTime = fTmp;
                    }
                    else
                    {
                    strErr = strprintf("LoadWallet() repairing tx ver=%d %s", wtx.fTimeReceivedIsTxTime, hash.ToString().c_str());
                        wtx.fTimeReceivedIsTxTime = 0;
                    }
                wss.vWalletUpgrade.push_back(hash);
                }

            if (wtx.nOrderPos == -1)
                wss.fAnyUnordered = true;

                //// debug print
                //printf("LoadWallet  %s\n", wtx.GetHash().ToString().c_str());
            //printf(" %12" PRI64d "  %s  %s  %s\n",
                //    wtx.vout[0].nValue,
                //    DateTimeStrFormat("%x %H:%M:%S", wtx.GetBlockTime()).c_str(),
                //    wtx.hashBlock.ToString().c_str(),
                //    wtx.mapValue["message"].c_str());
            }
            else if (strType == "acentry")
            {
                string strAccount;
                ssKey >> strAccount;
                uint64_t nNumber;
                ssKey >> nNumber;
                if (nNumber > nAccountingEntryNumber)
                    nAccountingEntryNumber = nNumber;
            // Added in NetcoinPOS
            if (!wss.fAnyUnordered)
            {
                CAccountingEntry acentry;
                ssValue >> acentry;
                if (acentry.nOrderPos == -1)
                    wss.fAnyUnordered = true;
            }
			// end added in NetcoinPOS

            }
            else if (strType == "key" || strType == "wkey")
            {
                // vector<unsigned char> vchPubKey;
                CPubKey vchPubKey;
                ssKey >> vchPubKey;
                if (!vchPubKey.IsValid())
                {
                    strErr = "Error reading wallet database: CPubKey corrupt";
                    return false;
                }
                CKey key;
                CPrivKey pkey;
                uint256 hash = 0;

                if (strType == "key")
                {
                wss.nKeys++;
                    // CPrivKey pkey;
                    ssValue >> pkey;
        /*            key.SetPubKey(vchPubKey);
                if (!key.SetPrivKey(pkey))
                {
                    strErr = "Error reading wallet database: CPrivKey corrupt";
                    return false;
                }
                    if (key.GetPubKey() != vchPubKey)
                    {
                    strErr = "Error reading wallet database: CPrivKey pubkey inconsistency";
                    return false;
                    }
                    if (!key.IsValid())
                    {
                    strErr = "Error reading wallet database: invalid CPrivKey";
                    return false;
                    }
                }
                else
                {
        */
                } else {
                    CWalletKey wkey;
                    ssValue >> wkey;
          /*          key.SetPubKey(vchPubKey);
                if (!key.SetPrivKey(wkey.vchPrivKey))
                {
                    strErr = "Error reading wallet database: CPrivKey corrupt";
                    return false;
                }
                    if (key.GetPubKey() != vchPubKey)
                    {
                    strErr = "Error reading wallet database: CWalletKey pubkey inconsistency";
                    return false;
                    }
                    if (!key.IsValid())
                    {
                    strErr = "Error reading wallet database: invalid CWalletKey";
                    return false;
                    }
          */
                    pkey = wkey.vchPrivKey;
                }
                // if (!key.SetPrivKey(pkey, vchPubKey.IsCompressed()))
                try
                {
                    // strErr = "Error reading wallet database: CPrivKey corrupt";
                    // return false;

                    ssValue >> hash;
                }
                catch(...){}

                bool fSkipCheck = false;

                if (hash != 0)
                {
                    // hash pubkey/privkey to accelerate wallet load
                    std::vector<unsigned char> vchKey;
                    vchKey.reserve(vchPubKey.size() + pkey.size());
                    vchKey.insert(vchKey.end(), vchPubKey.begin(), vchPubKey.end());
                    vchKey.insert(vchKey.end(), pkey.begin(), pkey.end());

                    if (Hash(vchKey.begin(), vchKey.end()) != hash)
                    {
                        strErr = "Error reading wallet database: CPubKey/CPrivKey corrupt";
                        return false;
                    }

                    fSkipCheck = true;
                }
                // if (key.GetPubKey() != vchPubKey)
                if (!key.Load(pkey, vchPubKey, fSkipCheck))
                {
                    // strErr = "Error reading wallet database: CPrivKey pubkey inconsistency";
                    strErr = "Error reading wallet database: CPrivKey corrupt";
                    return false;
                }
                // if (!pwallet->LoadKey(key))
                if (!pwallet->LoadKey(key, vchPubKey))
                {
                    strErr = "Error reading wallet database: LoadKey failed";
                    return false;
                }
            }
            else if (strType == "mkey")
            {
                unsigned int nID;
                ssKey >> nID;
                CMasterKey kMasterKey;
                ssValue >> kMasterKey;
                if(pwallet->mapMasterKeys.count(nID) != 0)
                {
                strErr = strprintf("Error reading wallet database: duplicate CMasterKey id %u", nID);
                return false;
                }
                pwallet->mapMasterKeys[nID] = kMasterKey;
                if (pwallet->nMasterKeyMaxID < nID)
                    pwallet->nMasterKeyMaxID = nID;
            }
            else if (strType == "ckey")
            {
            wss.nCKeys++;
                vector<unsigned char> vchPubKey;
                ssKey >> vchPubKey;
                vector<unsigned char> vchPrivKey;
                ssValue >> vchPrivKey;
                if (!pwallet->LoadCryptedKey(vchPubKey, vchPrivKey))
                {
                strErr = "Error reading wallet database: LoadCryptedKey failed";
                return false;
            }
            wss.fIsEncrypted = true;
                }
        else if (strType == "keymeta")
        {
            CPubKey vchPubKey;
            ssKey >> vchPubKey;
            CKeyMetadata keyMeta;
            ssValue >> keyMeta;
            wss.nKeyMeta++;

            pwallet->LoadKeyMetadata(vchPubKey, keyMeta);

            // find earliest key creation time, as wallet birthday
            if (!pwallet->nTimeFirstKey ||
                (keyMeta.nCreateTime < pwallet->nTimeFirstKey))
                pwallet->nTimeFirstKey = keyMeta.nCreateTime;
            }
            else if (strType == "defaultkey")
            {
                ssValue >> pwallet->vchDefaultKey;
            }
            else if (strType == "pool")
            {
                int64_t nIndex;
                ssKey >> nIndex;
				// Added in NetcoinPOS
            	CKeyPool keypool;
            	ssValue >> keypool;
				// End Added in NetcoinPOS
                pwallet->setKeyPool.insert(nIndex);

            // If no metadata exists yet, create a default with the pool key's
            // creation time. Note that this may be overwritten by actually
            // stored metadata for that key later, which is fine.
            CKeyID keyid = keypool.vchPubKey.GetID();
            if (pwallet->mapKeyMetadata.count(keyid) == 0)
                pwallet->mapKeyMetadata[keyid] = CKeyMetadata(keypool.nTime);

            }
            else if (strType == "version")
            {
            ssValue >> wss.nFileVersion;
            if (wss.nFileVersion == 10300)
                wss.nFileVersion = 300;
            }
            else if (strType == "cscript")
            {
                uint160 hash;
                ssKey >> hash;
                CScript script;
                ssValue >> script;
                if (!pwallet->LoadCScript(script))
                {
                strErr = "Error reading wallet database: LoadCScript failed";
                return false;
                }
            }
        else if (strType == "orderposnext")
        {
            ssValue >> pwallet->nOrderPosNext;
        }
		else if (strType == "s4c") // falsely names as autosavings, but for compat with older wallets keep using
        {
            string strS4CAccount;
            ssKey >> strS4CAccount;
            if (CBitcoinAddress(strS4CAccount).IsValid())
            {
                pwallet->fStakeForCharity = true;
                pwallet->strStakeForCharityAddress = CBitcoinAddress(strS4CAccount).Get();
                fGlobalStakeForCharity = true;
                string strS4CChhangeAccount;
                ssValue >> strS4CChhangeAccount;
                if (CBitcoinAddress(strS4CChhangeAccount).IsValid())
                    pwallet->strStakeForCharityChangeAddress = CBitcoinAddress(strS4CChhangeAccount).Get();
                ssValue >> pwallet->nStakeForCharityPercent;
            }

        }
        else if (strType == "s4c2")
        {
            string strS4CAccount;
            ssKey >> strS4CAccount;
            if (CBitcoinAddress(strS4CAccount).IsValid())
            {
                ssValue >> pwallet->nStakeForCharityMin;
                ssValue >> pwallet->nStakeForCharityMax;
            }
        }

    } catch (...)
    {
        return false;
    }
    return true;
}

static bool IsKeyType(string strType)
{
    return (strType== "key" || strType == "wkey" ||
            strType == "mkey" || strType == "ckey");
}

DBErrors CWalletDB::LoadWallet(CWallet* pwallet)
{
    pwallet->vchDefaultKey = CPubKey();
    CWalletScanState wss;
    bool fNoncriticalErrors = false;
    DBErrors result = DB_LOAD_OK;

    try {
        LOCK(pwallet->cs_wallet);
        int nMinVersion = 0;
        if (Read((string)"minversion", nMinVersion))
        {
            if (nMinVersion > CLIENT_VERSION)
                return DB_TOO_NEW;
            pwallet->LoadMinVersion(nMinVersion);
        }

        // Get cursor
        Dbc* pcursor = GetCursor();
        if (!pcursor)
        {
            printf("Error getting wallet database cursor\n");
            return DB_CORRUPT;
        }
        CCursorGuard cursorGuard(pcursor); // closes it on every exit path

        while (true)
        {
            // Read next record
            CDataStream ssKey(SER_DISK, CLIENT_VERSION);
            CDataStream ssValue(SER_DISK, CLIENT_VERSION);
            int ret = ReadAtCursor(pcursor, ssKey, ssValue);
            if (ret == DB_NOTFOUND)
                break;
            else if (ret != 0)
            {
                printf("Error reading next record from wallet database\n");
                return DB_CORRUPT;
            }

            // Try to be tolerant of single corrupt records:
            string strType, strErr;
            if (!ReadKeyValue(pwallet, ssKey, ssValue, wss, strType, strErr))
            {
                // losing keys is considered a catastrophic error, anything else
                // we assume the user can live with:
                if (IsKeyType(strType))
                    result = DB_CORRUPT;
                else
                {
                    // Leave other errors alone, if we try to fix them we might make things worse.
                    fNoncriticalErrors = true; // ... but do warn the user there is something wrong.
                    if (strType == "tx")
                        // Rescan if there is a bad transaction record:
                        SoftSetBoolArg("-rescan", true);
                }
            }
            if (!strErr.empty())
                printf("%s\n", strErr.c_str());
        }
        pcursor->close(); pcursor = NULL;
    }
    catch (boost::thread_interrupted) {
        throw;
    }
    catch (...) {
        result = DB_CORRUPT;
    }

    if (fNoncriticalErrors && result == DB_LOAD_OK)
        result = DB_NONCRITICAL_ERROR;

    // Any wallet corruption at all: skip any rewriting or
    // upgrading, we don't want to make it worse.
    if (result != DB_LOAD_OK)
        return result;

    printf("nFileVersion = %d\n", wss.nFileVersion);

    printf("Keys: %u plaintext, %u encrypted, %u w/ metadata, %u total\n",
           wss.nKeys, wss.nCKeys, wss.nKeyMeta, wss.nKeys + wss.nCKeys);

    // nTimeFirstKey is only reliable if all keys have metadata
    if ((wss.nKeys + wss.nCKeys) != wss.nKeyMeta)
        pwallet->nTimeFirstKey = 1; // 0 would be considered 'no value'


    BOOST_FOREACH(uint256 hash, wss.vWalletUpgrade)
        WriteTx(hash, pwallet->mapWallet[hash]);

    // Rewrite encrypted wallets of versions 0.4.0 and 0.5.0rc:
    if (wss.fIsEncrypted && (wss.nFileVersion == 40000 || wss.nFileVersion == 50000))
        return DB_NEED_REWRITE;

    if (wss.nFileVersion < CLIENT_VERSION) // Update
        WriteVersion(CLIENT_VERSION);

    if (wss.fAnyUnordered)
        result = ReorderTransactions(pwallet);

    return result;
}

void ThreadFlushWalletDB(const string& strFile)
{
    // Make this thread recognisable as the wallet flushing thread
    RenameThread("netcoin-wallet");

    // const string& strFile = ((const string*)parg)[0];
    static bool fOneThread;
    if (fOneThread)
        return;
    fOneThread = true;
    if (!GetBoolArg("-flushwallet", true))
        return;

    unsigned int nLastSeen = nWalletDBUpdated;
    unsigned int nLastFlushed = nWalletDBUpdated;
    int64_t nLastWalletUpdate = GetTime();
    while (true)
    {
        MilliSleep(500);

        if (nLastSeen != nWalletDBUpdated)
        {
            nLastSeen = nWalletDBUpdated;
            nLastWalletUpdate = GetTime();
        }

        if (nLastFlushed != nWalletDBUpdated && GetTime() - nLastWalletUpdate >= 2)
        {
            TRY_LOCK(bitdb.cs_db,lockDb);
            if (lockDb)
            {
                // Don't do this if any databases are in use
                int nRefCount = 0;
                map<string, int>::iterator mi = bitdb.mapFileUseCount.begin();
                while (mi != bitdb.mapFileUseCount.end())
                {
                    nRefCount += (*mi).second;
                    mi++;
                }

                if (nRefCount == 0)
                {
                    boost::this_thread::interruption_point();
                    map<string, int>::iterator mi = bitdb.mapFileUseCount.find(strFile);
                    if (mi != bitdb.mapFileUseCount.end())
                    {
                        printf("Flushing wallet.dat\n");
                        nLastFlushed = nWalletDBUpdated;
                        int64_t nStart = GetTimeMillis();

                        // Flush wallet.dat so it's self contained
                        bitdb.CloseDb(strFile);
                        bitdb.CheckpointLSN(strFile);

                        bitdb.mapFileUseCount.erase(mi++);
                        printf("Flushed wallet.dat %" PRI64d "ms\n", GetTimeMillis() - nStart);
                    }
                }
            }
        }
    }
}

bool BackupWallet(const CWallet& wallet, const string& strDest)
{
    if (!wallet.fFileBacked)
        return false;

    boost::filesystem::path pathDest(strDest);
    if (boost::filesystem::is_directory(pathDest))
        pathDest /= wallet.strWalletFile;

    try {
        CWalletDB walletdb(wallet.strWalletFile, "r");
        if (!walletdb.BackupTo(pathDest))
        {
            printf("error copying %s to %s\n", wallet.strWalletFile.c_str(), pathDest.string().c_str());
            return false;
        }
    } catch (const std::exception& e) {
        printf("error copying %s to %s - %s\n", wallet.strWalletFile.c_str(), pathDest.string().c_str(), e.what());
        return false;
    }
    printf("copied %s to %s\n", wallet.strWalletFile.c_str(), pathDest.string().c_str());
    return true;
}

//
// Try to (very carefully!) recover wallet.dat if there is a problem.
//
bool CWalletDB::Recover(CDBEnv& dbenv, std::string filename, bool fOnlyKeys)
{
    // Recovery procedure:
    // move wallet.dat to wallet.timestamp.bak
    // read every record that can still be read
    // write the salvaged data to a new wallet.dat
    // Set -rescan so any missing transactions will be
    // found.
    int64_t now = GetTime();
    std::string newFilename = strprintf("wallet.%" PRI64d ".bak", now);

    dbenv.CloseDb(filename);
    try {
        boost::filesystem::rename(dbenv.GetPath(filename), dbenv.GetPath(newFilename));
        printf("Renamed %s to %s\n", filename.c_str(), newFilename.c_str());
    } catch (const boost::filesystem::filesystem_error& e) {
        printf("Failed to rename %s to %s: %s\n", filename.c_str(), newFilename.c_str(), e.what());
        return false;
    }

    std::vector<CDBEnv::KeyValPair> salvagedData;
    bool allOK = dbenv.Salvage(newFilename, true, salvagedData);
    if (salvagedData.empty())
    {
        printf("Salvage found no records in %s.\n", newFilename.c_str());
        return false;
    }
    printf("Salvage found %" PRIszu " records\n", salvagedData.size());

    bool fSuccess = allOK;
    CWallet dummyWallet;
    CWalletScanState wss;
    try {
        CWalletDB db(filename, "cr+");
        db.TxnBegin();
        BOOST_FOREACH(CDBEnv::KeyValPair& row, salvagedData)
        {
            CDataStream ssKey(row.first, SER_DISK, CLIENT_VERSION);
            CDataStream ssValue(row.second, SER_DISK, CLIENT_VERSION);
            if (fOnlyKeys)
            {
                CDataStream ssKeyTmp(ssKey), ssValueTmp(ssValue);
                string strType, strErr;
                bool fReadOK = ReadKeyValue(&dummyWallet, ssKeyTmp, ssValueTmp,
                                            wss, strType, strErr);
                // besides the keys, keep the default key and key pool so a
                // (locked) encrypted wallet does not need new keys on startup
                if (!IsKeyType(strType) && strType != "defaultkey" && strType != "pool")
                    continue;
                if (!fReadOK)
                {
                    printf("WARNING: CWalletDB::Recover skipping %s: %s\n", strType.c_str(), strErr.c_str());
                    continue;
                }
            }
            if (!db.WriteRaw(ssKey, ssValue, true))
                fSuccess = false;
        }
        if (!db.TxnCommit())
            fSuccess = false;
    } catch (const std::exception& e) {
        printf("CWalletDB::Recover : %s\n", e.what());
        return false;
    }
    // release the exclusive lock so the file can be verified/reopened
    dbenv.CloseDb(filename);

    return fSuccess;
}

//
// One-time conversion of a legacy Berkeley DB wallet file to SQLite.
//
bool MigrateBerkeleyWallet(const std::string& strFile, std::string& strErr)
{
    boost::filesystem::path pathDataDir = GetDataDir();
    boost::filesystem::path pathWallet = pathDataDir / strFile;
    if (!IsBerkeleyDBFile(pathWallet))
        return true; // nothing to do

    printf("%s is a Berkeley DB wallet, converting it to SQLite\n", strFile.c_str());
    if (!HaveBerkeleyDBSupport())
    {
        strErr = "this build cannot read Berkeley DB wallets";
        return false;
    }

    std::vector<BerkeleyRecord> vRecords;
    if (!ReadBerkeleyDBWallet(pathDataDir, strFile, vRecords, strErr))
        return false;
    printf("Read %" PRIszu " records from the Berkeley DB wallet\n", vRecords.size());
    if (vRecords.empty())
    {
        strErr = "the wallet file contains no records";
        return false;
    }

    // keep the original file
    boost::filesystem::path pathBackup = pathDataDir / strprintf("%s.bdb-%" PRI64d ".bak", strFile.c_str(), GetTime());
    try {
        boost::filesystem::rename(pathWallet, pathBackup);
    } catch (const boost::filesystem::filesystem_error& e) {
        strErr = strprintf("cannot rename the old wallet: %s", e.what());
        return false;
    }

    bool fOk = false;
    try {
        {
            CWalletDB db(strFile, "cr+");
            fOk = db.TxnBegin();
            BOOST_FOREACH(const BerkeleyRecord& rec, vRecords)
            {
                CDataStream ssKey(rec.first, SER_DISK, CLIENT_VERSION);
                CDataStream ssValue(rec.second, SER_DISK, CLIENT_VERSION);
                fOk = fOk && db.WriteRaw(ssKey, ssValue, true);
            }
            fOk = fOk && db.TxnCommit();

            // read everything back and compare
            BOOST_FOREACH(const BerkeleyRecord& rec, vRecords)
            {
                if (!fOk)
                    break;
                CDataStream ssKey(rec.first, SER_DISK, CLIENT_VERSION);
                CDataStream ssValue(SER_DISK, CLIENT_VERSION);
                fOk = db.ReadRaw(ssKey, ssValue) &&
                      ssValue.size() == rec.second.size() &&
                      (rec.second.empty() || memcmp(&ssValue[0], &rec.second[0], rec.second.size()) == 0);
            }
            if (!fOk)
                strErr = "writing or verifying the converted wallet failed";
        }
        bitdb.CloseDb(strFile);
    } catch (const std::exception& e) {
        strErr = e.what();
        fOk = false;
    }

    // wipe the in-memory copies (they may contain private keys)
    BOOST_FOREACH(BerkeleyRecord& rec, vRecords)
    {
        if (!rec.second.empty())
            memset(&rec.second[0], 0, rec.second.size());
    }

    if (!fOk)
    {
        // restore the original
        bitdb.CloseDb(strFile);
        boost::system::error_code ec;
        boost::filesystem::remove(pathWallet, ec);
        boost::filesystem::rename(pathBackup, pathWallet, ec);
        return false;
    }

    printf("Converted %s to SQLite; the original is kept as %s\n", strFile.c_str(), pathBackup.filename().string().c_str());
    return true;
}

bool CWalletDB::Recover(CDBEnv& dbenv, std::string filename)
{
    return CWalletDB::Recover(dbenv, filename, false);
}
