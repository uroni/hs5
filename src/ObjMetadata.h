#pragma once

#include "ContentType.h"
#include "UserMetadata.h"
#include "data.h"
#include <chrono>
#include <memory>
#include <optional>
#include <string>

const uint32_t lockinfo_flag_lock_mode_compliance = 1 << 0;
const uint32_t lockinfo_flag_retain_until = 1 << 1;
const uint32_t lockinfo_flag_event_hold = 1 << 2;

class ObjMetadata
{
public:
    ContentType contentType;
    UserMetadata userMetadata;
    std::optional<std::string> disposition;
    std::optional<std::string> cacheControl;
    std::optional<std::string> encoding;
    std::optional<std::string> language;
    std::optional<std::string> expires;

    struct LockInfo
    {
        unsigned int flags = 0;
        std::chrono::system_clock::time_point retainUntil = std::chrono::system_clock::time_point{};
        unsigned int eventDurationDays = 0;
        unsigned short eventDurationYears = 0;

        bool complianceMode() const
        {
            return flags & lockinfo_flag_lock_mode_compliance;
        }

        bool retainUntilSet() const
        {
            return flags & lockinfo_flag_retain_until;
        }

        bool eventHold() const
        {
            return flags & lockinfo_flag_event_hold;
        }

        // How long the object stays protected after the event hold is released
        std::chrono::seconds eventHoldDuration() const
        {
            return std::chrono::days(eventDurationDays) + std::chrono::years(eventDurationYears);
        }

        // Object is protected from deletion at time 'now'
        bool isActive(const std::chrono::system_clock::time_point now) const
        {
            return eventHold() || (retainUntilSet() && retainUntil > now);
        }
    };

    std::optional<LockInfo> lockInfo;
    bool legalHold = false;

    static std::unique_ptr<ObjMetadata> fromHeaders(proxygen::HTTPMessage& headers, bool& invalid);

    int64_t flags() const;
    void serialize(CWData& wdata) const;
    bool deserialize(CRData& rdata, const int64_t flags);

    void addRespHeaders(proxygen::ResponseBuilder& resp);

    static void addRespHeaders(ObjMetadata* objMetadata, proxygen::ResponseBuilder& resp);
};