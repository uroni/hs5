/**
 * Copyright Martin Raiber. All Rights Reserved.
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */
#include <folly/String.h>
#include <string>
#include <assert.h>
#include <sodium.h>
#include <vector>

std::string cryptId(int64_t id, unsigned char* key)
{
    unsigned char nonce[crypto_secretbox_NONCEBYTES];
    randombytes_buf(nonce, sizeof(nonce));
    std::vector<unsigned char> encbuf(crypto_secretbox_MACBYTES + sizeof(id));
    if(crypto_secretbox_easy(&encbuf[0], reinterpret_cast<unsigned char*>(&id), sizeof(id), nonce, key)!=0)
        return std::string();

    return folly::hexlify(folly::ByteRange(nonce, sizeof(nonce))) +
        folly::hexlify(encbuf);
}

int64_t decryptId(const std::string& encryptedId, unsigned char* key)
{
    std::string binEncId;
    if(!folly::unhexlify(encryptedId, binEncId))
        return -1;

    int64_t ret;
    if(binEncId.size()!=crypto_secretbox_NONCEBYTES+crypto_secretbox_MACBYTES + sizeof(ret))
        return -1;

    const unsigned char* ptr = reinterpret_cast<const unsigned char*>(&binEncId[0]);

    if(crypto_secretbox_open_easy(reinterpret_cast<unsigned char*>(&ret), ptr+crypto_secretbox_NONCEBYTES, crypto_secretbox_MACBYTES + sizeof(ret), ptr, key)!=0)
        return -1;

    return ret;
}