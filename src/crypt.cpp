/**
 * Copyright Martin Raiber. All Rights Reserved.
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */
#include <folly/String.h>
#include <folly/base64.h>
#include <string>
#include <assert.h>
#include <sodium.h>
#include <vector>
#include "crypt.h"

std::string cryptId(int64_t id, unsigned char* key)
{
    unsigned char nonce[crypto_secretbox_NONCEBYTES];
    randombytes_buf(nonce, sizeof(nonce));
    std::vector<unsigned char> encbuf(crypto_secretbox_MACBYTES + sizeof(id));
    if(crypto_secretbox_easy(&encbuf[0], reinterpret_cast<unsigned char*>(&id), sizeof(id), nonce, key)!=0)
        return std::string();

    std::string_view nonce_view(reinterpret_cast<char*>(nonce), sizeof(nonce));
    std::string_view encbuf_view(reinterpret_cast<char*>(encbuf.data()), encbuf.size());

    return folly::base64URLEncode(nonce_view) +
        folly::base64URLEncode(encbuf_view);
}

CryptResult cryptIdSeparate(int64_t id, unsigned char* key, const std::string& preNonce)
{
    if(!preNonce.empty() && preNonce.size() != crypto_secretbox_NONCEBYTES)
        return CryptResult{};

    unsigned char nonceBuf[crypto_secretbox_NONCEBYTES];
    unsigned char* nonce = !preNonce.empty() ? reinterpret_cast<unsigned char*>(const_cast<char*>(preNonce.data())) : nonceBuf;
    if(preNonce.empty())
        randombytes_buf(nonceBuf, sizeof(nonceBuf));
    else if(preNonce.size() != crypto_secretbox_NONCEBYTES)
        return CryptResult{};
        
    std::vector<unsigned char> encbuf(crypto_secretbox_MACBYTES + sizeof(id));
    if(crypto_secretbox_easy(&encbuf[0], reinterpret_cast<unsigned char*>(&id), sizeof(id), nonce, key)!=0)
        return CryptResult{};

    std::string_view nonce_view(reinterpret_cast<char*>(nonce), sizeof(nonceBuf));
    std::string_view encbuf_view(reinterpret_cast<char*>(encbuf.data()), encbuf.size());

    return CryptResult{
        std::string(encbuf_view),
        std::string(nonce_view)
    };
}

int64_t decryptId(const std::string& encryptedId, unsigned char* key)
{
    std::string binEncId;
    try
    {
        binEncId = folly::base64URLDecode(encryptedId);
    }
    catch(folly::base64_decode_error&)
    {
        return -1;
    }

    int64_t ret;
    if(binEncId.size()!=crypto_secretbox_NONCEBYTES+crypto_secretbox_MACBYTES + sizeof(ret))
        return -1;

    const unsigned char* ptr = reinterpret_cast<const unsigned char*>(&binEncId[0]);

    if(crypto_secretbox_open_easy(reinterpret_cast<unsigned char*>(&ret), ptr+crypto_secretbox_NONCEBYTES, crypto_secretbox_MACBYTES + sizeof(ret), ptr, key)!=0)
        return -1;

    return ret;
}

int64_t decryptIdSeparate(const std::string& encryptedId, const std::string& nonce, unsigned char* key)
{
    if(nonce.size()!=crypto_secretbox_NONCEBYTES)
        return -1;

    int64_t ret;
    if(encryptedId.size()!=crypto_secretbox_MACBYTES + sizeof(ret))
        return -1;

    const unsigned char* encPtr = reinterpret_cast<const unsigned char*>(&encryptedId[0]);
    const unsigned char* noncePtr = reinterpret_cast<const unsigned char*>(&nonce[0]);

    if(crypto_secretbox_open_easy(reinterpret_cast<unsigned char*>(&ret), encPtr, crypto_secretbox_MACBYTES + sizeof(ret), noncePtr, key)!=0)
        return -1;

    return ret;
}