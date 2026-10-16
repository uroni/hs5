/**
 * Copyright Martin Raiber. All Rights Reserved.
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */
#include "ObjMetadata.h"
#include "s3handler.h"
#include "utils.h"

int64_t ObjMetadata::flags() const
{
    int64_t f = 0;
    if(contentType.hasContentType())
        f |= metadata_flag_with_content_type;
    if(userMetadata.hasUserMetadata())
        f |= metadata_flag_with_meta;
    if(disposition.has_value())
        f |= metadata_flag_with_disposition;
    if(cacheControl.has_value())
        f |= metadata_flag_with_cache_control;
    if(encoding.has_value())
        f |= metadata_flag_with_encoding;
    if(language.has_value())
        f |= metadata_flag_with_language;
    if(expires.has_value())
        f |= metadata_flag_with_expires;
    if(lockInfo.has_value())
        f |= metadata_flag_with_lock_info;
    if(legalHold)
        f |= metadata_flag_with_legal_hold;
    return f;
}

void ObjMetadata::serialize(CWData& wdata) const
{
    if(contentType.hasContentType())
        contentType.serialize(wdata);
    if(userMetadata.hasUserMetadata())
        userMetadata.serialize(wdata);
    if(disposition.has_value())
        wdata.addString2(*disposition);
    if(cacheControl.has_value())
        wdata.addString2(*cacheControl);
    if(encoding.has_value())
        wdata.addString2(*encoding);
    if(language.has_value())
        wdata.addString2(*language);
    if(expires.has_value())
        wdata.addString2(*expires);
    if(lockInfo.has_value())
    {
        wdata.addInt64(lockInfo->flags);
        if(lockInfo->retainUntilSet())
            wdata.addInt64(std::chrono::duration_cast<std::chrono::seconds>(lockInfo->retainUntil.time_since_epoch()).count());
        if(lockInfo->eventHold())
            wdata.addInt64(lockInfo->eventDurationDays*1000 + lockInfo->eventDurationYears);
    }
}

bool ObjMetadata::deserialize(CRData& rdata, const int64_t flags)
{
    if(flags & metadata_flag_with_content_type)
    {
        if(!contentType.deserialize(rdata))
            return false;
    }
    if(flags & metadata_flag_with_meta)
    {
        if(!userMetadata.deserialize(rdata))
            return false;
    }
    if(flags & metadata_flag_with_disposition)
    {
        std::string val;
        if(!rdata.getStr2(&val))
            return false;
        disposition = std::move(val);
    }
    if(flags & metadata_flag_with_cache_control)
    {
        std::string val;
        if(!rdata.getStr2(&val))
            return false;
        cacheControl = std::move(val);
    }
    if(flags & metadata_flag_with_encoding)
    {
        std::string val;
        if(!rdata.getStr2(&val))
            return false;
        encoding = std::move(val);

    }
    if(flags & metadata_flag_with_language)
    {
        std::string val;
        if(!rdata.getStr2(&val))
            return false;
        language = std::move(val);

    }
    if(flags & metadata_flag_with_expires)
    {
        std::string val;
        if(!rdata.getStr2(&val))
            return false;
        expires = std::move(val);
    }
    if(flags & metadata_flag_with_lock_info)
    {
        lockInfo.emplace();
        int64_t lockInfoFlags;
        if(!rdata.getInt64(&lockInfoFlags))
            return false;
        lockInfo->flags = static_cast<unsigned int>(lockInfoFlags);
        if(lockInfo->retainUntilSet())
        {
            int64_t retainUntilSec;
            if(!rdata.getInt64(&retainUntilSec))
                return false;
            lockInfo->retainUntil = std::chrono::system_clock::time_point(std::chrono::seconds(retainUntilSec));
        }
        if(lockInfo->eventHold())
        {
            int64_t eventDuration;
            if(!rdata.getInt64(&eventDuration))
                return false;
            lockInfo->eventDurationDays = eventDuration / 1000;
            lockInfo->eventDurationYears = eventDuration % 1000;
        }
    }
    if(flags & metadata_flag_with_legal_hold)
    {
        legalHold = true;
    }
    return true;
}

void ObjMetadata::addRespHeaders(proxygen::ResponseBuilder& resp)
{
    if(contentType.hasContentType())
        resp.header(proxygen::HTTP_HEADER_CONTENT_TYPE, contentTypeToStr(contentType));
    else
        resp.header(proxygen::HTTP_HEADER_CONTENT_TYPE, "binary/octet-stream");

    userMetadata.addRespHeaders(resp);
    if(disposition.has_value())
        resp.header(proxygen::HTTP_HEADER_CONTENT_DISPOSITION, *disposition);
    if(cacheControl.has_value())
        resp.header(proxygen::HTTP_HEADER_CACHE_CONTROL, *cacheControl);
    if(encoding.has_value())
        resp.header(proxygen::HTTP_HEADER_CONTENT_ENCODING, *encoding);
    if(language.has_value())
        resp.header(proxygen::HTTP_HEADER_CONTENT_LANGUAGE, *language);
    if(expires.has_value())
        resp.header(proxygen::HTTP_HEADER_EXPIRES, *expires);
    if(legalHold)
        resp.header("x-amz-object-lock-legal-hold", "ON");
    if(lockInfo.has_value())
    {
        if(lockInfo->complianceMode())
            resp.header("x-amz-object-lock-mode", "COMPLIANCE");
        else
            resp.header("x-amz-object-lock-mode", "GOVERNANCE");

        if(lockInfo->retainUntilSet())
            resp.header("x-amz-object-lock-retain-until-date", format_last_modified_rfc1123(lockInfo->retainUntil));

        if(lockInfo->eventHold())
        {
            resp.header("x-amz-object-lock-event-hold", "ON");
            resp.header("x-amz-object-lock-event-hold-duration-days", std::to_string(lockInfo->eventDurationDays));
            resp.header("x-amz-object-lock-event-hold-duration-years", std::to_string(lockInfo->eventDurationYears));
        }
    }

}

void ObjMetadata::addRespHeaders(ObjMetadata* objMetadata, proxygen::ResponseBuilder& resp)
{
    if(objMetadata)
        objMetadata->addRespHeaders(resp);
    else
        resp.header(proxygen::HTTP_HEADER_CONTENT_TYPE, "binary/octet-stream");
}

std::unique_ptr<ObjMetadata> ObjMetadata::fromHeaders(proxygen::HTTPMessage& headers, bool& invalid)
{
    UserMetadata userMetadata;
    if(!userMetadata.parseMetadataValues(headers))
    {
        invalid = true;
        return nullptr;
    }

    const auto contentTypeStr = headers.getHeaders().getSingleOrEmpty(proxygen::HTTP_HEADER_CONTENT_TYPE);
    ContentType contentType;
    if(!contentTypeStr.empty())
    {
        contentType = contentTypeFromStr(contentTypeStr);
    }

    const auto& dispositionStr = headers.getHeaders().getSingleOrEmpty(proxygen::HTTP_HEADER_CONTENT_DISPOSITION);
    const auto& cacheControlStr = headers.getHeaders().getSingleOrEmpty(proxygen::HTTP_HEADER_CACHE_CONTROL);
    auto encodingStr = headers.getHeaders().getSingleOrEmpty(proxygen::HTTP_HEADER_CONTENT_ENCODING);
    const auto& languageStr = headers.getHeaders().getSingleOrEmpty(proxygen::HTTP_HEADER_CONTENT_LANGUAGE);
    const auto& expiresStr = headers.getHeaders().getSingleOrEmpty(proxygen::HTTP_HEADER_EXPIRES);

    // Remove aws-chunked from encoding
    if(!encodingStr.empty())
    {
        std::vector<std::string_view> encodings;
        folly::split(',', encodingStr, encodings);
        std::string encodingsNew;
        for(auto& encoding: encodings)
        {
            if(folly::trimWhitespace(encoding) == "aws-chunked")
                continue;

            if(!encodingsNew.empty())
                encodingsNew += ",";
            encodingsNew += encoding;
        }
        encodingStr = encodingsNew;
    }


    if(!userMetadata.hasUserMetadata() && !contentType.hasContentType() && 
        dispositionStr.empty() && cacheControlStr.empty() && 
        encodingStr.empty() && languageStr.empty() && expiresStr.empty())
        return nullptr;

    auto metadata = std::make_unique<ObjMetadata>();
    metadata->userMetadata = std::move(userMetadata);
    metadata->contentType = std::move(contentType);
    if(!dispositionStr.empty())
        metadata->disposition = dispositionStr;
    if(!cacheControlStr.empty())
        metadata->cacheControl = cacheControlStr;
    if(!encodingStr.empty())
        metadata->encoding = encodingStr;
    if(!languageStr.empty())
        metadata->language = languageStr;
    if(!expiresStr.empty())
        metadata->expires = expiresStr;
    return metadata;
}