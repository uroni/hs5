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
#include "Policy.h"

namespace
{
    std::shared_mutex mutex;

    std::map<std::string, DbDao::AccessKey> accessKeys;

    std::map<int64_t, Policy> policies;

    struct Role
    {
        std::vector<Policy*> policies;
    };

    std::map<int64_t, Role> roles;

    struct User
    {
        DbDao::User user;
        std::vector<Role*> roles;
    };

    std::map<int64_t, User> users;

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

    const auto newPolicies = dao.getPolicies();
    policies.clear();
    for(const auto& policy: newPolicies)
    {
        policies.emplace(policy.id, Policy(policy.id));
    }

    const auto newRoles = dao.getRoles();
    roles.clear();
    for(const auto& role: newRoles)
    {
        const auto rolePolicies = dao.getRolePolicies(role.id);
        Role newRole;
        newRole.policies.reserve(rolePolicies.size());
        for(const auto& rolePolicy: rolePolicies)
        {
            auto itPolicy = policies.find(rolePolicy.policy_id);
            if(itPolicy != policies.end())
            {
                newRole.policies.push_back(&itPolicy->second);
            }
        }
        roles.insert(std::make_pair(role.id, std::move(newRole)));
    }

    auto newUsers = dao.getUsers();
    users.clear();
    for(auto& user: newUsers)
    {
        User newUser;
        newUser.user = std::move(user);

        auto userRoles = dao.getUserRoles(newUser.user.id);
        newUser.roles.reserve(userRoles.size());
        for(const auto& userRole: userRoles)
        {
            auto itRole = roles.find(userRole.role_id);
            if(itRole != roles.end())
            {
                newUser.roles.push_back(&itRole->second);
            }
        }
        users.insert(std::make_pair(user.id, std::move(newUser)));
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

bool isAuthorizedNoLock(const std::string_view resource, const Action action, const int64_t userId)
{
    auto userIt = users.find(userId);
    if(userIt == users.end())
        return false;

    const auto user = &userIt->second;
    
    Policy::AccessCheckResult finalRes = Policy::AccessCheckResult::NoMatch;
    for(const auto& role: user->roles)
    {
        for(const auto& policy: role->policies)
        {
            const auto res = policy->checkAccess(action, resource);
            if(res == Policy::AccessCheckResult::Deny)
                return false;
            else if(res == Policy::AccessCheckResult::Allow)
                finalRes = res;
        }
    }

    return finalRes == Policy::AccessCheckResult::Allow;
}

bool isAuthorized(const std::string_view resource, const Action action, const int64_t userId)
{
    std::shared_lock lock{mutex};
    return isAuthorizedNoLock(resource, action, userId);
}

bool isAuthorized(const std::string_view resource, const Action action, const std::string_view accessKey)
{
    std::shared_lock lock{mutex};

    const auto it = accessKeys.find(std::string(accessKey));

    if(it==accessKeys.end())
        return false;

    auto userId = it->second.user_id;
    if(userId == 0)
    {
        const auto sessionUserId = getSessionUserId(accessKey);
        if(!sessionUserId)
            return false;

        userId = *sessionUserId;
    }

    return isAuthorizedNoLock(resource, action, userId);
}

