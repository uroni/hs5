#pragma once
#include "data.h"
#include <proxygen/httpserver/ResponseBuilder.h>
#include <proxygen/lib/http/HTTPMessage.h>
#include <string>

struct UserMetadata
{
    std::vector<std::pair<std::string, std::string>> metadata;

    bool hasUserMetadata() const
    {
        return !metadata.empty();
    }

    bool deserialize(CRData& rdata);

    void serialize(CWData& wdata) const;

    bool parseMetadataValues(proxygen::HTTPMessage& headers);

    void addRespHeaders(proxygen::ResponseBuilder& resp);
};
