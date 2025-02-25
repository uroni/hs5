/**
 * Copyright Martin Raiber. All Rights Reserved.
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */
#include "Auth.h"
#include <mutex>
#include <shared_mutex>
#include <map>
#include "DbDao.h"
#include "Session.h"

namespace
{
    std::shared_mutex mutex;

    std::map<std::string, DbDao::AccessKey> accessKeys;
    std::map<int64_t, DbDao::User> users;
}

void refreshAuthCache()
{
    DbDao dao;

    std::scoped_lock lock{mutex};

    auto newAccessKeys = dao.getAccessKeys();
    accessKeys.clear();
    for(auto accessKey : newAccessKeys)
    {
        accessKeys.insert(std::make_pair(std::move(accessKey.key), std::move(accessKey)));
    }

    auto newUsers = dao.getUsers();
    users.clear();
    for(auto user: newUsers)
    {
        users.insert(std::make_pair(user.id, std::move(user)));
    }
}

void addAccessKey(const std::string_view accessKey, const std::string_view secretKey)
{
    std::scoped_lock lock{mutex};
    accessKeys.insert(std::make_pair(std::string(accessKey), DbDao::AccessKey{.key = std::string(accessKey), .secret_key = std::string(secretKey)}));
}

void removeAccessKey(const std::string_view accessKey)
{
    std::scoped_lock lock{mutex};
    accessKeys.erase(std::string(accessKey));
}

std::string getSecretKey(const std::string_view accessKey)
{
    std::shared_lock lock{mutex};

    const auto it = accessKeys.find(std::string(accessKey));

    if(it==accessKeys.end())
        return {};

    if(it->second.id == 0 && !hasSession(accessKey))
        return {};

    return it->second.secret_key;
}

bool isAuthorized(const std::string_view resource, const std::string_view action, const std::string_view accessKey)
{
    return true;
}

