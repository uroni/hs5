/**
 * Copyright Martin Raiber. All Rights Reserved.
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */
#include "Auth.h"
#include <mutex>
#include <shared_mutex>
#include <map>
#include "DbDao.h"


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

std::string getSecretKey(const std::string_view accessKey)
{
    std::shared_lock lock{mutex};

    const auto it = accessKeys.find(std::string(accessKey));

    if(it==accessKeys.end())
        return {};

    return it->second.secret_key;
}

bool isAuthorized(const std::string_view resource, const std::string_view action, const std::string_view accessKey)
{
    return true;
}

