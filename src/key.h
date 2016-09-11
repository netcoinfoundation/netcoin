// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2013 The Bitcoin developers
// Copyright (c) 2011-2012 Litecoin Developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
#ifndef BITCOIN_KEY_H
#define BITCOIN_KEY_H
#include <stdint.h>
// #include <stdexcept>
#include <vector>

#include "allocators.h"
#include "serialize.h"
#include "uint256.h"
// #include "util.h"
#include "hash.h"

// #include <openssl/ec.h> // for EC_KEY definition

// secp160k1
// const unsigned int PRIVATE_KEY_SIZE = 192;
// const unsigned int PUBLIC_KEY_SIZE  = 41;
// const unsigned int SIGNATURE_SIZE   = 48;
//
// secp192k1
// const unsigned int PRIVATE_KEY_SIZE = 222;
// const unsigned int PUBLIC_KEY_SIZE  = 49;
// const unsigned int SIGNATURE_SIZE   = 57;
//
// secp224k1
// const unsigned int PRIVATE_KEY_SIZE = 250;
// const unsigned int PUBLIC_KEY_SIZE  = 57;
// const unsigned int SIGNATURE_SIZE   = 66;
//
// secp256k1:
// const unsigned int PRIVATE_KEY_SIZE = 279;
// const unsigned int PUBLIC_KEY_SIZE  = 65;
// const unsigned int SIGNATURE_SIZE   = 72;
//
// see www.keylength.com
// script supports up to 75 for single byte push
/*
class key_error : public std::runtime_error
{
public:
    explicit key_error(const std::string& str) : std::runtime_error(str) {}
};
*/

/** A reference to a CKey: the Hash160 of its serialized public key */
class CKeyID : public uint160
{
public:
    CKeyID() : uint160(0) { }
    CKeyID(const uint160 &in) : uint160(in) { }
};

/** A reference to a CScript: the Hash160 of its serialization (see script.h) */
class CScriptID : public uint160
{
public:
    CScriptID() : uint160(0) { }
    CScriptID(const uint160 &in) : uint160(in) { }
};

/** An encapsulated public key. */
class CPubKey {
private:
    // std::vector<unsigned char> vchPubKey;

    // Just store the serialized data.
    // Its length can very cheaply be computed from the first byte.
    unsigned char vch[65];

    // Compute the length of a pubkey with a given first byte.
    unsigned int static GetLen(unsigned char chHeader) {
        if (chHeader == 2 || chHeader == 3)
            return 33;
        if (chHeader == 4 || chHeader == 6 || chHeader == 7)
            return 65;
        return 0;
    }

    // unsigned char *begin() {
    //    return vch;

    // Set this key data to be invalid
    void Invalidate() {
       vch[0] = 0xFF;
    }

    // friend class CKey;

public:
 /*   CPubKey() { }
    CPubKey(const std::vector<unsigned char> &vchPubKeyIn) : vchPubKey(vchPubKeyIn) { }
    friend bool operator==(const CPubKey &a, const CPubKey &b) { return a.vchPubKey == b.vchPubKey; }
    friend bool operator!=(const CPubKey &a, const CPubKey &b) { return a.vchPubKey != b.vchPubKey; }
    friend bool operator<(const CPubKey &a, const CPubKey &b) { return a.vchPubKey < b.vchPubKey; }
 */
 /*   CPubKey() { vch[0] = 0xFF; }

    CPubKey(const std::vector<unsigned char> &vchPubKeyIn) {
        int len = vchPubKeyIn.empty() ? 0 : GetLen(vchPubKeyIn[0]);
        if (len) {
            memcpy(vch, &vchPubKeyIn[0], len);
        } else {
            vch[0] = 0xFF;
        }
  */

    // Construct an invalid public key.
    CPubKey() {
        Invalidate();
    }
    // unsigned int size() const {
    //    return GetLen(vch[0]);

    // Initialize a public key using begin/end iterators to byte data.
    template<typename T>
    void Set(const T pbegin, const T pend) {
        int len = pend == pbegin ? 0 : GetLen(pbegin[0]);
        if (len && len == (pend-pbegin))
            memcpy(vch, (unsigned char*)&pbegin[0], len);
        else
            Invalidate();
    }

    // const unsigned char *begin() const {
    //    return vch;

    // Construct a public key using begin/end iterators to byte data.
    template<typename T>
    CPubKey(const T pbegin, const T pend) {
        Set(pbegin, pend);
    }
  /*
    IMPLEMENT_SERIALIZE(
        READWRITE(vchPubKey);
    )
*/
    // const unsigned char *end() const {
    //    return vch+size();

    // Construct a public key from a byte vector.
    CPubKey(const std::vector<unsigned char> &vch) {
        Set(vch.begin(), vch.end());
    }

    // friend bool operator==(const CPubKey &a, const CPubKey &b) { return memcmp(a.vch, b.vch, a.size()) == 0; }
    // friend bool operator!=(const CPubKey &a, const CPubKey &b) { return memcmp(a.vch, b.vch, a.size()) != 0; }
    // Simply read-only vector-like interface to the pubkey data.
    unsigned int size() const { return GetLen(vch[0]); }
    const unsigned char *begin() const { return vch; }
    const unsigned char *end() const { return vch+size(); }
    const unsigned char &operator[](unsigned int pos) const { return vch[pos]; }

    // Comparator implementation.
    friend bool operator==(const CPubKey &a, const CPubKey &b) {
        return a.vch[0] == b.vch[0] &&
                memcmp(a.vch, b.vch, a.size()) == 0;
    }
    friend bool operator!=(const CPubKey &a, const CPubKey &b) {
        return !(a == b);
    }
    friend bool operator<(const CPubKey &a, const CPubKey &b) {
        return a.vch[0] < b.vch[0] ||
                // (a.vch[0] == b.vch[0] && memcmp(a.vch+1, b.vch+1, a.size() - 1) < 0);
                (a.vch[0] == b.vch[0] && memcmp(a.vch, b.vch, a.size()) < 0);
    }

    // Implement serialization, as if this was a byte vector.
    unsigned int GetSerializeSize(int nType, int nVersion) const {
        return size() + 1;
    }
    template<typename Stream> void Serialize(Stream &s, int nType, int nVersion) const {
        unsigned int len = size();
        ::Serialize(s, VARINT(len), nType, nVersion);
        s.write((char*)vch, len);
    }
    template<typename Stream> void Unserialize(Stream &s, int nType, int nVersion) {
        unsigned int len;
        ::Unserialize(s, VARINT(len), nType, nVersion);
        if (len <= 65) {
            s.read((char*)vch, len);
        } else {

            // invalid pubkey
            // vch[0] = 0xFF;

            // invalid pubkey, skip available data
            char dummy;
            while (len--)
                s.read(&dummy, 1);
            Invalidate();
        }
    }

    // Get the KeyID of this public key (hash of its serialization)
    CKeyID GetID() const {
        // return CKeyID(Hash160(vchPubKey));
        return CKeyID(Hash160(vch, vch+size()));
    }

    // Get the 256-bit hash of this public key.
    uint256 GetHash() const {
        // return Hash(vchPubKey.begin(), vchPubKey.end());
        return Hash(vch, vch+size());
    }

    // just check syntactic correctness.
    bool IsValid() const {
        // return vchPubKey.size() == 33 || vchPubKey.size() == 65;
        return size() > 0;
    }

    // fully validate whether this is a valid public key (more expensive than IsValid())
    bool IsFullyValid() const;

    // Check whether this is a compressed public key.
    bool IsCompressed() const {
        // return vchPubKey.size() == 33;
        return size() == 33;
    }

    // std::vector<unsigned char> Raw() const {
    //    // return vchPubKey;
    //    return std::vector<unsigned char>(vch, vch+size());
    // }

    // Verify a DER signature (~72 bytes).
    // If this public key is not fully valid, the return value will be false.
    bool Verify(const uint256 &hash, const std::vector<unsigned char>& vchSig) const;

    // Verify a compact signature (~65 bytes).
    // See CKey::SignCompact.
    bool VerifyCompact(const uint256 &hash, const std::vector<unsigned char>& vchSig) const;

    // Recover a public key from a compact signature.
    bool RecoverCompact(const uint256 &hash, const std::vector<unsigned char>& vchSig);

    // Turn this public key into an uncompressed public key.
    bool Decompress();
};


// secure_allocator is defined in serialize.h
// CPrivKey is a serialized private key, with all parameters included (279 bytes)
typedef std::vector<unsigned char, secure_allocator<unsigned char> > CPrivKey;
// CSecret is a serialization of just the secret parameter (32 bytes)
// typedef std::vector<unsigned char, secure_allocator<unsigned char> > CSecret;

/*
/** An encapsulated OpenSSL Elliptic Curve key (public and/or private) *
class CKey
{
protected:
    EC_KEY* pkey;
    bool fSet;
    bool fCompressedPubKey;

    // void SetCompressedPubKey();
*/

/** An encapsulated private key. */
class CKey {
private:
    // Whether this private key is valid. We check for correctness when modifying the key
    // data, so fValid should always correspond to the actual state.
    bool fValid;

    // Whether the public key corresponding to this private key is (to be) compressed.
    bool fCompressed;

    // The actual byte data
    unsigned char vch[32];

    // Check whether the 32-byte array pointed to be vch is valid keydata.
    bool static Check(const unsigned char *vch);
public:
    // void SetCompressedPubKey(bool fCompressed = true);

    // void Reset();

    // Construct an invalid private key.
    CKey() : fValid(false) {
        LockObject(vch);
    }

    // Copy constructor. This is necessary because of memlocking.
    CKey(const CKey &secret) : fValid(secret.fValid), fCompressed(secret.fCompressed) {
        LockObject(vch);
        memcpy(vch, secret.vch, sizeof(vch));
    }

    // Destructor (again necessary because of memlocking).
    ~CKey() {
        UnlockObject(vch);
    }

    // Initialize using begin and end iterators to byte data.
    template<typename T>
    void Set(const T pbegin, const T pend, bool fCompressedIn) {
        if (pend - pbegin != 32) {
            fValid = false;
            return;
        }
        if (Check(&pbegin[0])) {
            memcpy(vch, (unsigned char*)&pbegin[0], 32);
            fValid = true;
            fCompressed = fCompressedIn;
        } else {
            fValid = false;
        }
    }

    // CKey();
    // CKey(const CKey& b);

    // Simple read-only vector-like interface.
     unsigned int size() const { return (fValid ? 32 : 0); }
    const unsigned char *begin() const { return vch; }
    const unsigned char *end() const { return vch + size(); }

    // CKey& operator=(const CKey& b);

    // Check whether this private key is valid.
    bool IsValid() const { return fValid; }

    // ~CKey();

    // Check whether the public key corresponding to this private key is (to be) compressed.
    bool IsCompressed() const { return fCompressed; }

    // bool IsNull() const;
    // bool IsCompressed() const;

    // Initialize from a CPrivKey (serialized OpenSSL private key data).
    bool SetPrivKey(const CPrivKey &vchPrivKey, bool fCompressed);

    void MakeNewKey(bool fCompressed);
    // bool SetPrivKey(const CPrivKey& vchPrivKey);
    // bool SetSecret(const CSecret& vchSecret, bool fCompressed = false);
    // CSecret GetSecret(bool &fCompressed) const;

    // Convert the private key to a CPrivKey (serialized OpenSSL private key data).
    // This is expensive.
    CPrivKey GetPrivKey() const;

    // bool SetPubKey(const CPubKey& vchPubKey);

    // Compute the public key from a private key.
    // This is expensive.
    CPubKey GetPubKey() const;

    // bool Sign(uint256 hash, std::vector<unsigned char>& vchSig);

    // Create a DER-serialized signature.
    bool Sign(const uint256 &hash, std::vector<unsigned char>& vchSig) const;

    // Create a compact signature (65 bytes), which allows reconstructing the used public key
    // The format is one header byte, followed by two times 32 bytes for the serialized r and s values.
    // The header byte: 0x1B = first key with even y, 0x1C = first key with odd y,
    //                  0x1D = second key with even y, 0x1E = second key with odd y
  //  bool SignCompact(uint256 hash, std::vector<unsigned char>& vchSig);

    // reconstruct public key from a compact signature
    // This is only slightly more CPU intensive than just verifying it.
    // If this function succeeds, the recovered public key is guaranteed to be valid
    // (the signature is a valid signature of the given data for that key)
  //  bool SetCompactSignature(uint256 hash, const std::vector<unsigned char>& vchSig);

 //   bool Verify(uint256 hash, const std::vector<unsigned char>& vchSig);

    // Verify a compact signature
 //   bool VerifyCompact(uint256 hash, const std::vector<unsigned char>& vchSig);

 //   bool IsValid();

    //                  0x1D = second key with even y, 0x1E = second key with odd y,
    //                  add 0x04 for compressed keys.
    bool SignCompact(const uint256 &hash, std::vector<unsigned char>& vchSig) const;

    // Check whether an element of a signature (r or s) is valid.
    static bool CheckSignatureElement(const unsigned char *vch, int len, bool half);

    // Ensure that signature is DER-encoded
    static bool ReserealizeSignature(std::vector<unsigned char>& vchSig);
};

#endif
