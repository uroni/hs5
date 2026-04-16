#pragma once

#include "ContentType.h"
#include "UserMetadata.h"
#include <optional>
#include <string>
#include <memory>
#include "data.h"

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


    static std::unique_ptr<ObjMetadata> fromHeaders(proxygen::HTTPMessage& headers, bool& invalid);

    int64_t flags() const;
    void serialize(CWData& wdata) const;
    bool deserialize(CRData& rdata, const int64_t flags);

    void addRespHeaders(proxygen::ResponseBuilder& resp);

    static void addRespHeaders(ObjMetadata* objMetadata, proxygen::ResponseBuilder& resp);
};