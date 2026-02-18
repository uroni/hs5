#pragma once
#include <string>
#include <string_view>
#include "data.h"

struct ContentType
{
    int contentType = 0;
    std::string contentTypeStr;

    bool hasContentType() const
    {
        return contentType != 0 || !contentTypeStr.empty();
    }

    size_t serializeSize() const;

    void serialize(char* buffer) const;

    bool deserialize(CRData& rdata);

    void serialize(CWData& wdata) const;
};

std::string contentTypeToStr(const ContentType& contentType);
ContentType contentTypeFromStr(const std::string_view contentTypeStr);