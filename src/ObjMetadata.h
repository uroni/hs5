#pragma once

#include "ContentType.h"
#include "UserMetadata.h"
#include <optional>
#include <string>
#include <memory>
#include <chrono>
#include "data.h"

const uint32_t lockinfo_flag_lock_mode_compliance = 1 << 0;
const uint32_t lockinfo_flag_retain_until= 1 << 1;
const uint32_t lockinfo_flag_event_hold = 1 << 1;


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


        bool complianceMode() const {
            return flags & lockinfo_flag_lock_mode_compliance;
        }
        
        bool retainUntilSet() const {
            return flags & lockinfo_flag_retain_until;
        }

        bool eventHold() const {
            return flags & lockinfo_flag_event_hold;
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