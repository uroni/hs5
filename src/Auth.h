#pragma once

#include <string>

void refreshAuthCache();

void addAccessKey(const std::string_view accessKey, const std::string_view secretKey);

void removeAccessKey(const std::string_view accessKey);

std::string getSecretKey(const std::string_view accessKey);

bool isAuthorized(const std::string_view resource, const std::string_view action, const std::string_view accessKey);

