/**
 * Copyright Martin Raiber. All Rights Reserved.
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */
#include "Session.h"
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

    constexpr std::chrono::minutes sessionTimeout = 15min;
}


void newSession(const std::string_view jsSes, const std::string_view cookieSes, ApiSessionStorage apiSessionStorage)
{
    std::scoped_lock lock{mutex};

    SessionStorage sessionStorage = {.apiStorage = std::move(apiSessionStorage)};

    sessionData[fmt::format("{}{}",jsSes, cookieSes)] = std::move(sessionStorage);

    auto now = std::chrono::steady_clock::now();
    for(auto it = sessionData.begin();it!=sessionData.end();)
    {
        if(!it->second.locked && now - it->second.usedAt > sessionTimeout)
            it = sessionData.erase(it);
        else
            ++it;
    }
}

SessionScope getSession(const std::string_view jsSes, const std::string_view cookieSes)
{
    std::unique_lock lock{mutex};

    while(true)
    {
        auto it = sessionData.find(fmt::format("{}{}",jsSes, cookieSes));

        if(it == sessionData.end())
            return SessionScope(nullptr);

        if(it->second.locked)
        {
            unlockCond.wait(lock);
        }

        auto now = std::chrono::steady_clock::now();
        if(now - it->second.usedAt > sessionTimeout)
        {
            sessionData.erase(it);
            return SessionScope(nullptr);
        }

        it->second.usedAt = std::chrono::steady_clock::now();

        return SessionScope(&it->second);
    }

    assert(false);
    return SessionScope(nullptr);
}

void unlockSession(SessionStorage& storage)
{
    std::scoped_lock lock(mutex);
    storage.locked=false;
    unlockCond.notify_all();
}
