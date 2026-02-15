/**
 * Copyright Martin Raiber. All Rights Reserved.
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */
#include "Session.h"
#include "Auth.h"
#include <condition_variable>
#include <mutex>
#include <map>
#include <fmt/core.h>
#include <assert.h>

using namespace std::chrono_literals;

namespace
{
    std::mutex mutex;
    std::condition_variable unlockCond;

    std::map<std::string, SessionStorage> sessionData;
    std::map<std::string, std::map<std::string, SessionStorage>::iterator> accessKeyToSession;

    constexpr std::chrono::minutes sessionTimeout = 15min;
}

std::map<std::string, SessionStorage>::iterator removeSession(std::map<std::string, SessionStorage>::iterator it)
{
    if(!it->second.accessKey.empty())
    {
        removeAccessKey(it->second.accessKey);
    }

    accessKeyToSession.erase(it->second.accessKey);

    return sessionData.erase(it);
}


void newSession(const std::string_view jsSes, const std::string_view cookieSes, const std::string_view accessKey, const std::string_view secretAccessKey, ApiSessionStorage apiSessionStorage)
{
    addAccessKey(accessKey, secretAccessKey);

    std::scoped_lock lock{mutex};

    SessionStorage sessionStorage = {.apiStorage = std::move(apiSessionStorage), .accessKey = std::string(accessKey)};

    auto itInsert = sessionData.insert(std::make_pair(fmt::format("{}{}", jsSes, cookieSes), std::move(sessionStorage)));
    accessKeyToSession[std::string(accessKey)] = itInsert.first;

    auto now = std::chrono::steady_clock::now();
    for(auto it = sessionData.begin();it!=sessionData.end();)
    {
        if(!it->second.locked && now - it->second.usedAt > sessionTimeout)
            it = removeSession(it);
        else
            ++it;
    }
}

SessionScope getSession(const std::string_view jsSes, const std::string_view cookieSes)
{
    std::unique_lock lock{mutex};

    auto it = sessionData.find(fmt::format("{}{}",jsSes, cookieSes));

    if(it == sessionData.end())
        return SessionScope(nullptr);

    while(true)
    {
        if(it->second.locked)
        {
            unlockCond.wait(lock);

            it = sessionData.find(fmt::format("{}{}",jsSes, cookieSes));
            if(it == sessionData.end())
                return SessionScope(nullptr);
        }

        const auto now = std::chrono::steady_clock::now();
        if(now - it->second.usedAt > sessionTimeout)
        {
            removeSession(it);
            return SessionScope(nullptr);
        }

        if(!it->second.locked)
        {
            it->second.usedAt = std::chrono::steady_clock::now();

            return SessionScope(&it->second);
        }
    }

    assert(false);
    return SessionScope(nullptr);
}

bool invalidateSession(const std::string_view jsSes, const std::string_view cookieSes)
{
    std::unique_lock lock{mutex};

    auto it = sessionData.find(fmt::format("{}{}",jsSes, cookieSes));

    if(it == sessionData.end())
        return false;

    while(true)
    {
        if(it->second.locked)
        {
            unlockCond.wait(lock);
        }

        it = sessionData.find(fmt::format("{}{}",jsSes, cookieSes));

        if(it == sessionData.end())
            return true;
        
        if(!it->second.locked)
        {
            removeSession(it);
            return true;
        }
    }
    assert(false);
    return true;
}

void unlockSession(SessionStorage& storage)
{
    std::scoped_lock lock(mutex);
    storage.locked=false;
    unlockCond.notify_all();
}

SessionStorage* getSessionStorage(const std::string_view accessKey)
{
    const auto it = accessKeyToSession.find(std::string(accessKey));
    if(it==accessKeyToSession.end())
        return nullptr;
    
    const auto now = std::chrono::steady_clock::now();
    if(now - it->second->second.usedAt > sessionTimeout)
        return nullptr;

    it->second->second.usedAt = now;

    return &it->second->second;
}

bool hasSession(const std::string_view accessKey)
{
    std::scoped_lock lock(mutex);
    return getSessionStorage(accessKey) != nullptr;
}

std::optional<int64_t> getSessionUserId(const std::string_view accessKey)
{
    std::scoped_lock lock(mutex);
    auto* sessionStorage = getSessionStorage(accessKey);
    if(sessionStorage==nullptr)
        return {};

    return sessionStorage->apiStorage.userId;
}