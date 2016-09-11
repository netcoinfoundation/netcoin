// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2012 The Bitcoin developers
// Copyright (c) 2011-2012 Litecoin Developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
#ifndef BITCOIN_WALLETDB_H
#define BITCOIN_WALLETDB_H

#include <stdint.h>
#include "db.h"
#include "base58.h"

class CKeyPool;
class CAccount;
class CAccountingEntry;
class CWallet;
class CWalletTx;


/** Error statuses for the wallet database */
enum DBErrors
{
    DB_LOAD_OK,
    DB_CORRUPT,
    DB_NONCRITICAL_ERROR,
    DB_TOO_NEW,
    DB_LOAD_FAIL,
    DB_NEED_REWRITE
};

class CKeyMetadata
{
public:
    static const int CURRENT_VERSION=1;
    int nVersion;
    int64_t nCreateTime; // 0 means unknown

    CKeyMetadata()
    {
        SetNull();
    }
    CKeyMetadata(int64_t nCreateTime_)
    {
        nVersion = CKeyMetadata::CURRENT_VERSION;
        nCreateTime = nCreateTime_;
    }

    IMPLEMENT_SERIALIZE
    (
        READWRITE(this->nVersion);
        nVersion = this->nVersion;
        READWRITE(nCreateTime);
    )

    void SetNull()
    {
        nVersion = CKeyMetadata::CURRENT_VERSION;
        nCreateTime = 0;
    }
};


/** Access to the wallet database (wallet.dat) */
class CWalletDB : public CDB
{
public:
    // CWalletDB(std::string strFilename, const char* pszMode="r+") : CDB(strFilename.c_str(), pszMode)
    CWalletDB(const std::string& strFilename, const char* pszMode = "r+") : CDB(strFilename, pszMode)
    {
    }

private:
    CWalletDB(const CWalletDB&);
    void operator=(const CWalletDB&);

public:
    bool ReadName(const std::string& strAddress, std::string& strName)
    {
        strName = "";
        return Read(std::make_pair(std::string("name"), strAddress), strName);
    }

    bool WriteName(const std::string& strAddress, const std::string& strName);

    bool EraseName(const std::string& strAddress);

    bool ReadTx(uint256 hash, CWalletTx& wtx)
    {
        return Read(std::make_pair(std::string("tx"), hash), wtx);
    }

    bool WriteTx(uint256 hash, const CWalletTx& wtx)
    {
        nWalletDBUpdated++;
        return Write(std::make_pair(std::string("tx"), hash), wtx);
    }

    bool EraseTx(uint256 hash)
    {
        nWalletDBUpdated++;
        return Erase(std::make_pair(std::string("tx"), hash));
    }

    bool ReadKey(const CPubKey& vchPubKey, CPrivKey& vchPrivKey)
    {
        vchPrivKey.clear();
        return Read(std::make_pair(std::string("key"), vchPubKey), vchPrivKey);
    }

    bool WriteKey(const CPubKey& vchPubKey, const CPrivKey& vchPrivKey, const CKeyMetadata &keyMeta)
    {
        nWalletDBUpdated++;
        // Added in NetcoinPOS
        if(!Write(std::make_pair(std::string("keymeta"), vchPubKey), keyMeta))
            return false;

        // return Write(std::make_pair(std::string("key"), vchPubKey), vchPrivKey, false);

        // hash pubkey/privkey to accelerate wallet load
        std::vector<unsigned char> vchKey;
        vchKey.reserve(vchPubKey.size() + vchPrivKey.size());
        vchKey.insert(vchKey.end(), vchPubKey.begin(), vchPubKey.end());
        vchKey.insert(vchKey.end(), vchPrivKey.begin(), vchPrivKey.end());

        return Write(std::make_pair(std::string("key"), vchPubKey), std::make_pair(vchPrivKey, Hash(vchKey.begin(), vchKey.end())), false);
    }

    bool WriteCryptedKey(const CPubKey& vchPubKey, const std::vector<unsigned char>& vchCryptedSecret, const CKeyMetadata &keyMeta)
    {
        nWalletDBUpdated++;
        bool fEraseUnencryptedKey = true;
        // added in NetCoinPos
        if(!Write(std::make_pair(std::string("keymeta"), vchPubKey), keyMeta))
            return false;

        if (!Write(std::make_pair(std::string("ckey"), vchPubKey), vchCryptedSecret, false))
            return false;
        if (fEraseUnencryptedKey)
        {
            Erase(std::make_pair(std::string("key"), vchPubKey));
            Erase(std::make_pair(std::string("wkey"), vchPubKey));
        }
        return true;
    }

    bool WriteMasterKey(unsigned int nID, const CMasterKey& kMasterKey)
    {
        nWalletDBUpdated++;
        return Write(std::make_pair(std::string("mkey"), nID), kMasterKey, true);
    }

    // Support for BIP 0013 : see https://en.bitcoin.it/wiki/BIP_0013
    bool ReadCScript(const uint160 &hash, CScript& redeemScript)
    {
        redeemScript.clear();
        return Read(std::make_pair(std::string("cscript"), hash), redeemScript);
    }

    bool WriteCScript(const uint160& hash, const CScript& redeemScript)
    {
        nWalletDBUpdated++;
        return Write(std::make_pair(std::string("cscript"), hash), redeemScript, false);
    }

    bool WriteBestBlock(const CBlockLocator& locator)
    {
        nWalletDBUpdated++;
        return Write(std::string("bestblock"), locator);
    }

    bool ReadBestBlock(CBlockLocator& locator)
    {
        return Read(std::string("bestblock"), locator);
    }

    bool ReadDefaultKey(std::vector<unsigned char>& vchPubKey)
    {
        vchPubKey.clear();
        return Read(std::string("defaultkey"), vchPubKey);
    }

    bool WriteOrderPosNext(int64_t nOrderPosNext)
    {
        nWalletDBUpdated++;
        return Write(std::string("orderposnext"), nOrderPosNext);
    }

    bool WriteDefaultKey(const CPubKey& vchPubKey)
    {
        nWalletDBUpdated++;
        return Write(std::string("defaultkey"), vchPubKey);
    }

    bool ReadPool(int64_t nPool, CKeyPool& keypool)
    {
        return Read(std::make_pair(std::string("pool"), nPool), keypool);
    }

    bool WritePool(int64_t nPool, const CKeyPool& keypool)
    {
        nWalletDBUpdated++;
        return Write(std::make_pair(std::string("pool"), nPool), keypool);
    }

    bool ErasePool(int64_t nPool)
    {
        nWalletDBUpdated++;
        return Erase(std::make_pair(std::string("pool"), nPool));
    }

    // Settings are no longer stored in wallet.dat; these are
    // used only for backwards compatibility:
    template<typename T>
    bool ReadSetting(const std::string& strKey, T& value)
    {
        return Read(std::make_pair(std::string("setting"), strKey), value);
    }
    template<typename T>
    bool WriteSetting(const std::string& strKey, const T& value)
    {
        nWalletDBUpdated++;
        return Write(std::make_pair(std::string("setting"), strKey), value);
    }
    bool EraseSetting(const std::string& strKey)
    {
        nWalletDBUpdated++;
        return Erase(std::make_pair(std::string("setting"), strKey));
    }

    bool WriteMinVersion(int nVersion)
    {
        return Write(std::string("minversion"), nVersion);
    }

    bool WriteStakeForCharity(std::string strStakeForCharityAddress,
                              int nStakeForCharityPercent,
                              std::string strStakeForCharityChangeAddress,
                              int64_t nStakeForCharityMinAmount,
                              int64_t nStakeForCharityMaxAmount)
    {
        nWalletDBUpdated++;
        if (!Write(std::make_pair(std::string("s4c"), strStakeForCharityAddress),std::make_pair(strStakeForCharityChangeAddress ,nStakeForCharityPercent)))
            return false;

        return Write(std::make_pair(std::string("s4c2"), strStakeForCharityAddress),std::make_pair(nStakeForCharityMinAmount ,nStakeForCharityMaxAmount));

    }

    bool EraseStakeForCharity(std::string strStakeForCharityAddress)
    {
        nWalletDBUpdated++;

        return Erase(std::make_pair(std::string("s4c"), strStakeForCharityAddress));
        return Erase(std::make_pair(std::string("s4c2"), strStakeForCharityAddress));
    }



    bool ReadAccount(const std::string& strAccount, CAccount& account);
    bool WriteAccount(const std::string& strAccount, const CAccount& account);
private:
    bool WriteAccountingEntry(const uint64_t nAccEntryNum, const CAccountingEntry& acentry);
public:
    bool WriteAccountingEntry(const CAccountingEntry& acentry);
    int64_t GetAccountCreditDebit(const std::string& strAccount);
    void ListAccountCreditDebit(const std::string& strAccount, std::list<CAccountingEntry>& acentries);

    DBErrors ReorderTransactions(CWallet*);
    DBErrors LoadWallet(CWallet* pwallet);
    static bool Recover(CDBEnv& dbenv, std::string filename, bool fOnlyKeys);
    static bool Recover(CDBEnv& dbenv, std::string filename);
};

bool BackupWallet(const CWallet& wallet, const std::string& strDest);

#endif // BITCOIN_WALLETDB_H
