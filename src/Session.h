#pragma once

#include <string>
#include <chrono>
#include <utility>

struct ApiSessionStorage
{
    int64_t userId = 0;
};

struct SessionStorage
{
    ApiSessionStorage apiStorage;
    std::string accessKey;
    std::chrono::time_point<std::chrono::steady_clock> usedAt = std::chrono::steady_clock::now();
    bool locked = false;
};

void newSession(const std::string_view jsSes, const std::string_view cookieSes, const std::string_view accessKey, const std::string_view secretAccessKey, ApiSessionStorage apiSessionStorage);

void unlockSession(SessionStorage& storage);

class SessionScope
{
    SessionStorage* storage;

public:
    SessionScope(SessionStorage* storage)
        : storage(storage) {}
    ~SessionScope()
    {
        if(storage!=nullptr)
            unlockSession(*storage);
    }

    SessionScope(const SessionScope& other) = delete;

    SessionScope& operator=(const SessionScope& other) = delete;

    SessionScope(SessionScope&& other) noexcept
        : storage(std::exchange(other.storage, nullptr)) {}

    SessionScope& operator=(SessionScope&& other) noexcept
    {
        if (this != &other)
        {
            storage = std::exchange(other.storage, nullptr);
        }
        return *this;
    }
    operator bool() const
    {
        return storage!=nullptr;
    }

    ApiSessionStorage& operator ->() const
    {
        return storage->apiStorage;
    }

    ApiSessionStorage& operator*() const
    {
        return storage->apiStorage;
    }
};

SessionScope getSession(const std::string_view jsSes, const std::string_view cookieSes);

bool hasSession(const std::string_view accessKey);


