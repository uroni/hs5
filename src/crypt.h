#pragma once

#include <string>

std::string cryptId(int64_t id, unsigned char* key);

struct CryptResult
{
    std::string encryptedId;
    std::string nonce;
};

CryptResult cryptIdSeparate(int64_t id, unsigned char* key, const std::string& preNonce = std::string());

int64_t decryptId(const std::string& encryptedId, unsigned char* key);

int64_t decryptIdSeparate(const std::string& encryptedId, const std::string& nonce, unsigned char* key);