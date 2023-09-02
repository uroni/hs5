#pragma once

#include <string>

std::string cryptId(int64_t id, unsigned char* key);

int64_t decryptId(const std::string& encryptedId, unsigned char* key);