#pragma once
#include "data.h"
#include <string>
#include <string_view>

struct ContentType
{
    int contentType = 0;
    std::string contentTypeStr;

    bool hasContentType() const
    {
        return contentType != 0 || !contentTypeStr.empty();
    }

    bool deserialize(CRData& rdata);

    void serialize(CWData& wdata) const;
};

std::string contentTypeToStr(const ContentType& contentType);
ContentType contentTypeFromStr(const std::string_view contentTypeStr);