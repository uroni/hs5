/**
 * Copyright Martin Raiber. All Rights Reserved.
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */
#include <folly/String.h>
#include <folly/base64.h>
#include <string>
#include <assert.h>
#include <sodium.h>
#include <openssl/evp.h>
#include <cstring>
#include <optional>
#include "crypt.h"

namespace
{
    constexpr size_t aesBlockSize = 16;
    static_assert(aesBlockSize == cryptIdSize);

    struct AesCtx
    {
        ~AesCtx()
        {
            EVP_CIPHER_CTX_free(ctx);
        }

        EVP_CIPHER_CTX* ctx = nullptr;
        const unsigned char* key = nullptr;
    };

    thread_local AesCtx encCtx;
    thread_local AesCtx decCtx;

    // Returns thread local cipher context initialized with key. ECB without
    // padding keeps no state between full blocks, so it can be reused.
    EVP_CIPHER_CTX* getAesCtx(const unsigned char* key, bool encrypt)
    {
        static_assert(crypto_secretbox_KEYBYTES == 32);

        AesCtx& aesCtx = encrypt ? encCtx : decCtx;

        // Key memory is owned by SingleFileStorage and does not change
        if(aesCtx.ctx!=nullptr && aesCtx.key==key)
            return aesCtx.ctx;

        if(aesCtx.ctx==nullptr)
        {
            aesCtx.ctx = EVP_CIPHER_CTX_new();
            if(aesCtx.ctx==nullptr)
                return nullptr;
        }

        if(EVP_CipherInit_ex(aesCtx.ctx, EVP_aes_256_ecb(), nullptr, key, nullptr, encrypt ? 1 : 0)!=1 ||
            EVP_CIPHER_CTX_set_padding(aesCtx.ctx, 0)!=1)
        {
            EVP_CIPHER_CTX_free(aesCtx.ctx);
            aesCtx.ctx = nullptr;
            return nullptr;
        }

        aesCtx.key = key;
        return aesCtx.ctx;
    }

    // Single AES-256 block over id || 8 zero bytes. Deterministic; the zero
    // padding acts as a 64-bit authentication tag on decryption.
    bool aesBlock(const unsigned char* in, unsigned char* out, const unsigned char* key, bool encrypt)
    {
        EVP_CIPHER_CTX* ctx = getAesCtx(key, encrypt);
        if(ctx==nullptr)
            return false;

        int outl = 0;
        return EVP_CipherUpdate(ctx, out, &outl, in, aesBlockSize)==1 &&
            outl==static_cast<int>(aesBlockSize);
    }

    bool encryptIdBlock(int64_t id, unsigned char* out, const unsigned char* key)
    {
        unsigned char plain[aesBlockSize] = {};
        memcpy(plain, &id, sizeof(id));
        return aesBlock(plain, out, key, true);
    }

    int64_t decryptIdBlock(const unsigned char* in, const unsigned char* key)
    {
        unsigned char plain[aesBlockSize];
        if(!aesBlock(in, plain, key, false))
            return -1;

        static const unsigned char zeros[aesBlockSize - sizeof(int64_t)] = {};
        if(sodium_memcmp(plain + sizeof(int64_t), zeros, sizeof(zeros))!=0)
            return -1;

        int64_t ret;
        memcpy(&ret, plain, sizeof(ret));
        return ret;
    }

    std::optional<std::string> decodeBase64URL(const std::string& data)
    {
        try
        {
            return folly::base64URLDecode(data);
        }
        catch(folly::base64_decode_error&)
        {
            return std::nullopt;
        }
    }
}

std::string cryptId(int64_t id, unsigned char* key)
{
    unsigned char enc[aesBlockSize];
    if(!encryptIdBlock(id, enc, key))
        return std::string();

    return folly::base64URLEncode(std::string_view(reinterpret_cast<char*>(enc), sizeof(enc)));
}

int64_t decryptId(const std::string& encryptedId, unsigned char* key)
{
    const auto binEncId = decodeBase64URL(encryptedId);
    if(!binEncId || binEncId->size()!=aesBlockSize)
        return -1;

    return decryptIdBlock(reinterpret_cast<const unsigned char*>(binEncId->data()), key);
}
