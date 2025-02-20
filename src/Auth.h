#pragma once

#include <string>

void refreshAuthCache();

std::string getSecretKey(const std::string_view accessKey);

bool isAuthorized(const std::string_view resource, const std::string_view action, const std::string_view accessKey);

