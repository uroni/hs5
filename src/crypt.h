#pragma once

#include <cstddef>
#include <string>

// Size of the binary (not base64 encoded) encrypted id
constexpr size_t cryptIdSize = 16;

std::string cryptId(int64_t id, unsigned char* key);

int64_t decryptId(const std::string& encryptedId, unsigned char* key);