#pragma once

#include <cstddef>
#include <string>

// Size of the binary (not base64 encoded) encrypted id
constexpr size_t cryptIdSize = 16;

std::string cryptId(int64_t id, unsigned char* key);

// Binary (cryptIdSize bytes) ETag from version. Cannot be decrypted via decryptId
std::string cryptEtag(int64_t version, unsigned char* key);

int64_t decryptId(const std::string& encryptedId, unsigned char* key);