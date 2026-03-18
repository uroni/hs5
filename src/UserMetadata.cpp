/**
 * Copyright Martin Raiber. All Rights Reserved.
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */
#include "UserMetadata.h"
#include "utils.h"
#include <folly/logging/LogLevel.h>
#include <folly/logging/xlog.h>

bool UserMetadata::deserialize(CRData& rdata)
{
    int64_t cvals = 0;
    if(!rdata.getVarInt(&cvals))
        return false;

    for(int64_t i=0; i<cvals; ++i)
    {
        std::string key, value;
        if(!rdata.getStr2(&key) || !rdata.getStr2(&value))
            return false;
        metadata.push_back(std::make_pair(key, value));
    }
    return true;
}

void UserMetadata::serialize(CWData& wdata) const
{
    wdata.addVarInt(metadata.size());

    for(const auto& [k, v]: metadata)
    {
        wdata.addString2(k);
        wdata.addString2(v);
    }
}

bool UserMetadata::parseMetadataValues(proxygen::HTTPMessage& headers)
{
    size_t usedSize = 0;
    std::unordered_map<std::string, size_t> metaKeys;
    headers.getHeaders().forEach([&](const std::string& header, const std::string& value){
        const auto headerLower = asciiToLower(header);
        if(headerLower.starts_with("x-amz-meta-"))
        {
            const auto metaKey = headerLower.substr(11);
            usedSize+= metaKey.size();
            std::string metaVal = rfc2047Decode(value);
            usedSize+= metaVal.size();
            auto itOffs = metaKeys.find(metaKey);
            if(itOffs != metaKeys.end())
            {
                metadata[itOffs->second].second+=", "+ metaVal;
            }
            else
            {
                metaKeys[metaKey] = metadata.size();
                metadata.push_back(std::make_pair(metaKey, metaVal));
            }
        }
    });
    if(usedSize > 2*1024)
    {
        XLOGF(INFO, "Total size of user metadata for object is {}, exceeding limit. Returning error", usedSize);
        return false;
    }
    return true;
}

void UserMetadata::addRespHeaders(proxygen::ResponseBuilder& resp)
{
    for(const auto& [k, v]: metadata)
    {
        resp.header("x-amz-meta-"+k, rfc2047EncodeB(v));
    }
}