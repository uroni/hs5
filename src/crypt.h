#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>

// Size of the binary (not base64 encoded) encrypted id
constexpr size_t cryptIdSize = 16;

std::string cryptId(int64_t id, unsigned char* key);

// Binary (cryptIdSize bytes) ETag from version. Cannot be decrypted via decryptId
std::string cryptEtag(int64_t version, unsigned char* key);

// Returns -1 if decryption fails
int64_t decryptId(const std::string& encryptedId, unsigned char* key);

// Returns std::nullopt if decryption fails. Can return any (also negative) id
std::optional<int64_t> decryptIdOpt(const std::string& encryptedId, unsigned char* key);