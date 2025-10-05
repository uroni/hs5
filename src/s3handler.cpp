/**
 * Copyright Martin Raiber. All Rights Reserved.
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */
#include "s3handler.h"
#include "ApiHandler.h"
#include "SingleFileStorage.h"
#include <algorithm>
#include <asm-generic/errno-base.h>
#include <asm-generic/errno.h>
#include <atomic>
#include <cstdint>
#include <cstdlib>
#include <expat.h>
#include <folly/Format.h>
#include <folly/Range.h>
#include <folly/ScopeGuard.h>
#include <folly/String.h>
#include <folly/executors/GlobalExecutor.h>
#include <folly/io/IOBuf.h>
#include <folly/io/IOBufQueue.h>
#include <folly/logging/LogLevel.h>
#include <folly/logging/xlog.h>
#include <folly/Uri.h>
#include <folly/lang/Bits.h>
#include <folly/hash/Hash.h>
#include <folly/base64.h>
#include <limits>
#include <memory>
#include <openssl/hmac.h>
#include <openssl/sha.h>
#include <openssl/md5.h>
#include <openssl/types.h>
#include <optional>
#include <proxygen/lib/http/HTTPCommonHeaders.h>
#include <proxygen/lib/http/HTTPMethod.h>
#include "data.h"
#include "utils.h"
#include <limits.h>
#include <string_view>
#include "Auth.h"
#include "main.h"

using namespace proxygen;

const char c_commit_uuid[] = "a711e93e-93b4-4a9e-8a0b-688797470002";
const char c_stop_uuid[] = "3db7da22-8ce2-4420-a8ca-f09f0b8e0e61";
const char unsigned_payload[] = "UNSIGNED-PAYLOAD";

DEFINE_bool(with_stop_command, false, "Allow stopping via putting to stop object");
DEFINE_bool(allow_sig_v2, true, "Allow aws sig v2");
DEFINE_string(host_override, "", "Override host for s3 requests");
DEFINE_bool(pre_sync_commit, false, "Pre-sync data and index files before commit for potential performance gain");
DEFINE_int64(commit_after_ms, 30000, "If manual commit is enabled, wait this amount of milliseconds before automatically committing. 0 means not commiting automatically at all.");

std::string hashSha256Hex(const std::string_view payload)
{
    unsigned char md[SHA256_DIGEST_LENGTH];
    SHA256(reinterpret_cast<const unsigned char *>(payload.data()),
           payload.size(), md);
    return folly::hexlify<std::string>(
        folly::ByteRange(md, SHA256_DIGEST_LENGTH));
}

const std::string emptyPayloadHash = hashSha256Hex("");

std::string hmacSha256Binary(const std::string &key,
                             const std::string &payload)
{
    std::string ret;
    ret.resize(SHA256_DIGEST_LENGTH);
    unsigned int len = SHA256_DIGEST_LENGTH;
    HMAC(EVP_sha256(), key.data(), key.size(),
         reinterpret_cast<const unsigned char *>(payload.data()), payload.size(),
         reinterpret_cast<unsigned char *>(&ret[0]), &len);
    assert(len == SHA256_DIGEST_LENGTH);
    return ret;
}

const size_t SHA1_DIGEST_LENGTH = 20;

std::string hmacSha1Binary(const std::string &key,
                             const std::string &payload)
{
    std::string ret;
    ret.resize(SHA1_DIGEST_LENGTH);
    unsigned int len = SHA1_DIGEST_LENGTH;
    HMAC(EVP_sha1(), key.data(), key.size(),
         reinterpret_cast<const unsigned char *>(payload.data()), payload.size(),
         reinterpret_cast<unsigned char *>(&ret[0]), &len);
    assert(len == SHA1_DIGEST_LENGTH);
    return ret;
}

std::string getDate(const std::tm& final_tm)
{
    std::array<char, 10> buffer;
    std::strftime(buffer.data(), buffer.size(), "%Y%m%d", &final_tm);
    return buffer.data();
}


std::string uploadIdToStr(int64_t uploadId)
{
    std::string key;
	if(uploadId<UCHAR_MAX)
	{
		key.resize(sizeof(unsigned char)+1);
		unsigned char bnum = folly::Endian::big(static_cast<unsigned char>(uploadId));
		memcpy(&key[1], &bnum, sizeof(bnum));
	}
	else if(uploadId<USHRT_MAX)
	{
		key.resize(sizeof(unsigned short)+1);
		unsigned short bnum = folly::Endian::big(static_cast<unsigned short>(uploadId));
		memcpy(&key[1], &bnum, sizeof(bnum));
	}
	else if(uploadId<UINT_MAX)
	{
		key.resize(sizeof(unsigned int)+1);
		unsigned int bnum = folly::Endian::big(static_cast<unsigned int>(uploadId));
		memcpy(&key[1], &bnum, sizeof(bnum));
	}
	else
	{
		key.resize(sizeof(int64_t)+1);	
        uploadId = folly::Endian::big(uploadId);
		memcpy(&key[1], &uploadId, sizeof(uploadId));
	}
	return key;
}

std::string encodeUriAws(const std::string_view str)
{
    static char hexValues[] = "0123456789ABCDEF";

    std::string ret;
    ret.reserve(str.size());
    for(const auto& ch: str)
    {
        if((ch>='A' && ch<='Z') ||
            (ch>='a' && ch<='z') ||
            (ch>='0' && ch<='9') ||
            ch=='-' || ch=='.' || ch=='_' || ch=='~')
        {
            ret+=ch;   
        }
        else
        {
            ret+='%';
            ret += hexValues[(ch >> 4) & 0xf];
            ret += hexValues[ch & 0xf];
        }
    }
    return ret;
}

std::string asciiToLower(std::string str)
{
    std::transform(str.begin(), str.end(), str.begin(),
        [](auto c) { return std::tolower(c); });
    return str;
}

std::string getCanonicalizedAmzHeaders(const HTTPHeaders& headers)
{
    std::map<std::string, std::string> amzHeaders;
    headers.forEach([&](const std::string& header, const std::string& val) {
        const auto& headerLower = asciiToLower(header);
        if (headerLower.find("x-amz-") == 0)
        {
            amzHeaders[headerLower] = val;
        }
    });

    std::string canonicalizedAmzHeaders;

    for (const auto& header : amzHeaders)
    {
        canonicalizedAmzHeaders += header.first + ":" + header.second + "\n";
    }

    return canonicalizedAmzHeaders;
}

std::string getCanonicalizedResource(const HTTPMessage& headers)
{
    std::string canonicalizedResource;
    canonicalizedResource += headers.getPathAsStringPiece();
    const auto& queryParams = headers.getQueryParams();

    const static std::vector<std::string> resources = {"acl", "lifecycle", "location", "logging", "notification", "partNumber", "policy", 
        "requestPayment", "uploadId", "uploads", "versionId", "versioning", "versions", "website"};

    bool hasResource = false;
    for(const auto& resource: resources)    
    {
        auto it = queryParams.find(resource);
        if(it!=queryParams.end())
        {
            if(!hasResource)
            {
                canonicalizedResource += "?";
                hasResource = true;
            }
            else
            {
                canonicalizedResource += "&";
            }
            canonicalizedResource += resource;
            if(!it->second.empty())
                canonicalizedResource += "=" + it->second;
        }
    }

    // Include  response-content-type, response-content-language, response-expires, response-cache-control, response-content-disposition, and response-content-encoding.
    const static std::vector<std::string> responseParams = {"response-content-type", "response-content-language", "response-expires", "response-cache-control", "response-content-disposition", "response-content-encoding"};
    for(const auto& responseParam: responseParams)
    {
        auto it = queryParams.find(responseParam);
        if(it!=queryParams.end())
        {
            if(!hasResource)
            {
                canonicalizedResource += "?";
                hasResource = true;
            }
            else
            {
                canonicalizedResource += "&";
            }
            canonicalizedResource += responseParam + "=" + it->second;
        }
    }

    // Include delete param
    auto it = queryParams.find("delete");
    if(it!=queryParams.end())
    {
        if(!hasResource)
        {
            canonicalizedResource += "?";
            hasResource = true;
        }
        else
        {
            canonicalizedResource += "&";
        }
        canonicalizedResource += "delete=" + it->second;
    }
    return canonicalizedResource;
}

std::optional<std::string> checkSigQueryStringV2(HTTPMessage &headers, const std::string_view ressource,
              const std::string_view action)
{
    const auto httpVerb = headers.getMethodString(); 

    const auto& queryParams = headers.getQueryParams();
    const auto itAccessKey = queryParams.find("AWSAccessKeyId");
    if(itAccessKey==queryParams.end())
        return std::nullopt;

    const auto& accessKey = itAccessKey->second;

    const auto& secretKey = getSecretKey(accessKey);

    if(secretKey.empty())
        return std::nullopt;

    if(!ressource.empty() && !isAuthorized(ressource, action, accessKey))
        return std::nullopt;

    const auto itExpires = queryParams.find("Expires");
    if(itExpires==queryParams.end())
        return std::nullopt;

    const auto expires = std::stoll(itExpires->second);

    if(expires < std::time(nullptr))
        return std::nullopt;

    const auto itSignature = queryParams.find("Signature");
    if(itSignature==queryParams.end())
        return std::nullopt;

    const auto signature = itSignature->second;

    const auto canonicalizedAmzHeaders = getCanonicalizedAmzHeaders(headers.getHeaders());

    const auto canonicalizedResource = getCanonicalizedResource(headers);

    const auto stringToSign = folly::sformat(
        "{}\n{}\n{}\n{}\n{}{}", httpVerb, headers.getHeaders().getSingleOrEmpty("Content-MD5"),
        headers.getHeaders().getSingleOrEmpty(proxygen::HTTP_HEADER_CONTENT_LENGTH), expires, canonicalizedAmzHeaders,
        canonicalizedResource);

    const auto urlSig = folly::base64Encode(hmacSha1Binary(secretKey, stringToSign));

    try
    {
        if(urlSig != folly::uriUnescape<std::string>(signature))
            return std::nullopt;
    }
    catch(std::invalid_argument&)
    {
        return std::nullopt;
    }
    
    return itAccessKey->second;
}

std::optional<std::string> checkSig(HTTPMessage &headers,
              const std::string_view payload,
              const std::string_view ressource,
              const std::string_view action,
              const bool allowPresigned)
{
    const char alg_name[] = "AWS4-HMAC-SHA256";
    constexpr std::string_view alg = "AWS4-HMAC-SHA256 ";

    const auto& authorization = headers.getHeaders().getSingleOrEmpty(proxygen::HTTP_HEADER_AUTHORIZATION);

    std::map<std::string, std::string> unescapedParams;   
    for(auto& param:  headers.getQueryParams())
    {
        try
        {
            unescapedParams[param.first] = folly::uriUnescape<std::string>(param.second, folly::UriEscapeMode::QUERY);
        } catch (const std::invalid_argument& ex) {
            XLOGF(WARN, "Invalid escaped query param: {}", folly::exceptionStr(ex));
        }
    }

    folly::StringPiece requestSignature;
    std::vector<folly::StringPiece> credentialScopeToks;
    std::vector<folly::StringPiece> signedHeadersVec;
    folly::StringPiece signedHeaders;
    std::string requestDateTime;
    long long expires = 0;
    
    const bool presignedRequest = authorization.empty() && allowPresigned;
    if (presignedRequest)
    {
        const auto itAlg = unescapedParams.find("X-Amz-Algorithm");
        if(itAlg==unescapedParams.end() || itAlg->second != alg_name)
        {
            if(!FLAGS_allow_sig_v2)
                return std::nullopt;

            return checkSigQueryStringV2(headers, ressource, action);
        }

        const auto itCredential = unescapedParams.find("X-Amz-Credential");
        if(itCredential==unescapedParams.end())
            return std::nullopt;

        folly::split('/', itCredential->second, credentialScopeToks);

        const auto itDate = unescapedParams.find("X-Amz-Date");
        if(itDate==unescapedParams.end())
            return std::nullopt;
        
        requestDateTime = itDate->second;

        const auto itExpires = unescapedParams.find("X-Amz-Expires");
        if(itExpires==unescapedParams.end())
            return std::nullopt;

        expires = std::stoll(itExpires->second);
        if(expires<1 || expires>604800)
            return std::nullopt;

        const auto itSignedHeaders = unescapedParams.find("X-Amz-SignedHeaders");
        if(itSignedHeaders==unescapedParams.end())
            return std::nullopt;

        signedHeaders = itSignedHeaders->second;
        folly::split(';', signedHeaders, signedHeadersVec);

        const auto itSignature = unescapedParams.find("X-Amz-Signature");
        if(itSignature==unescapedParams.end())
            return std::nullopt;

        requestSignature = itSignature->second;
    }
    else
    {
        if (authorization.find(alg) != 0)
            return std::nullopt;

        std::vector<folly::StringPiece> authorizationVec;
        folly::split(',', std::string_view(authorization).substr(alg.size()), authorizationVec);

        std::map<folly::StringPiece, folly::StringPiece> authorizationMap;

        for (auto ave : authorizationVec)
        {
            size_t eq = ave.find_first_of('=');
            if (eq != std::string::npos)
            {
                authorizationMap.insert(
                    std::make_pair(trimWhitespace(ave.subpiece(0, eq)), trimWhitespace(ave.subpiece(eq + 1))));
            }
        }

        const char signedHeadersKey[] = "SignedHeaders";
        auto itSignedHeaders = authorizationMap.find(signedHeadersKey);
        if (itSignedHeaders == authorizationMap.end())
            return std::nullopt;

        signedHeaders = itSignedHeaders->second;
        folly::split(';', signedHeaders, signedHeadersVec);

        const char credentialHeaderKey[] = "Credential";
        auto itCredential = authorizationMap.find(credentialHeaderKey);
        if (itCredential == authorizationMap.end())
            return std::nullopt;

        folly::split('/', itCredential->second, credentialScopeToks);

        const char signatureHeaderKey[] = "Signature";
        auto itSignature = authorizationMap.find(signatureHeaderKey);
        if (itSignature == authorizationMap.end())
            return std::nullopt;

        requestSignature = itSignature->second;        
    }

    if (credentialScopeToks.size() != 5)
        return std::nullopt;

    const auto& accessKey = credentialScopeToks[0];

    // TODO: get from credentialScopeToks[0]
    const auto& secretKey = getSecretKey(accessKey);

    if(secretKey.empty())
        return std::nullopt;

    if(!ressource.empty() && !isAuthorized(ressource, action, accessKey))
        return std::nullopt;

    std::string canonicalHeaders;
    std::optional<folly::Range<const char *> > prevSignedHeader;
    bool hasHost = false;
    std::string usedHost;
    for (auto signedHeader : signedHeadersVec)
    {
        if (prevSignedHeader && *prevSignedHeader >= signedHeader)
        {
            return std::nullopt;
        }
        auto fullVal = headers.getHeaders().getSingleOrEmpty(signedHeader);
        auto val = folly::trimWhitespace(fullVal);
        if (signedHeader == "host" && !val.empty())
        {
            hasHost = true;
            if(!FLAGS_host_override.empty())
                val = FLAGS_host_override;
            usedHost = val;            
        }
        canonicalHeaders += folly::sformat("{}:{}\n", signedHeader, val);
        prevSignedHeader = signedHeader;
    }

    if (!hasHost)
        return std::nullopt;
    
    std::vector<std::string> encodedParams;
    encodedParams.reserve(unescapedParams.size());
    size_t paramsStrLen=0;
    for (auto param : unescapedParams)
    {   
        if(presignedRequest && param.first == "X-Amz-Signature")
            continue;

        try
        {
            encodedParams.emplace_back(encodeUriAws(param.first) + "=" +
                             encodeUriAws(param.second));
            paramsStrLen+=encodedParams.back().size()+1;
        } catch (const std::invalid_argument& ex) {
            XLOGF(WARN, "Invalid escaped query param: {}", folly::exceptionStr(ex));
        }
    }

    std::sort(encodedParams.begin(), encodedParams.end());

    std::string canonicalParamStr;
    canonicalParamStr.reserve(paramsStrLen);
    for(const auto param: encodedParams)
    {
        if (!canonicalParamStr.empty())
            canonicalParamStr += "&";
        
        canonicalParamStr += param;
    }

    std::string emptyPayload;
    if(payload.empty())
    {
        // We expect an empty payload, but we also don't read the body, so don't care if this is wrong
        emptyPayload = headers.getHeaders().getSingleOrEmpty("x-amz-content-sha256");
    
        if(!emptyPayload.empty() &&
            emptyPayload != unsigned_payload &&
            emptyPayload != emptyPayloadHash)
        {
            XLOGF(WARN, "Unkown empty payload '{}' (not empty or UNSIGNED-PAYLOAD). Ignoring", emptyPayload);
        }
    }

    const auto canonicalRequest = folly::sformat(
        "{}\n{}\n{}\n{}\n{}\n{}", headers.getMethodString(),
        headers.getPathAsStringPiece(), canonicalParamStr, canonicalHeaders,
        signedHeaders, presignedRequest ? unsigned_payload : (payload.empty() ? 
            (emptyPayload.empty() ? unsigned_payload : emptyPayload) : payload ));

    const auto hashedCanonicalRequest = hashSha256Hex(canonicalRequest);
    if(requestDateTime.empty())
    {
        requestDateTime =
            headers.getHeaders().getSingleOrEmpty("X-Amz-Date");

        if(requestDateTime.empty())
        {
            requestDateTime = headers.getHeaders().getSingleOrEmpty(HTTP_HEADER_DATE);
        }
    }
    
    if(requestDateTime.empty())
        return std::nullopt;

    std::tm t = {};
    std::istringstream ss(requestDateTime);
    ss >> std::get_time(&t, "%Y%m%dT%H%M%SZ");
    struct tm tm = t;
    const auto requestTime = timegm(&tm);
    const auto currTime = std::time(nullptr);
    
    if(presignedRequest)
    {
        if(expires==0)
            return std::nullopt;

        if(requestTime + expires < currTime)
            return std::nullopt;
    }
    else
    {
        if(expires!=0)
            return std::nullopt;

        const auto timeDist = requestTime > currTime ? (requestTime - currTime ) : (currTime - requestTime);

        if(timeDist > 8*60)
            return std::nullopt;
    }

    const auto stringToSign = folly::sformat(
        "{}\n{}\n{}/{}/{}/{}\n{}", alg_name, requestDateTime,
        credentialScopeToks[1], credentialScopeToks[2], credentialScopeToks[3],
        credentialScopeToks[4], hashedCanonicalRequest);

    const auto dateKey = hmacSha256Binary("AWS4" + secretKey, getDate(t));
    const auto dateRegionKey = hmacSha256Binary(dateKey, credentialScopeToks[2].toString());
    const auto dateRegionServiceKey = hmacSha256Binary(dateRegionKey, credentialScopeToks[3].toString());
    const auto signingKey = hmacSha256Binary(dateRegionServiceKey, "aws4_request");

    const auto calculatedSignature = folly::hexlify(hmacSha256Binary(signingKey, stringToSign));

    if(calculatedSignature != requestSignature)
    {
        XLOGF(INFO, "Signature mismatch for {}?{} method {} access key {} resource {} action {} payload hash {} expected {} got {} used host={}", headers.getPathAsStringPiece(),
             headers.getQueryStringAsStringPiece(), headers.getMethodString(), accessKey, ressource, action, payload, calculatedSignature, requestSignature, usedHost);
        return std::nullopt;
    }

    XLOGF(INFO, "Signature OK for {}?{} method {} access key {} resource {} action {} payload hash {}", headers.getPathAsStringPiece(), headers.getQueryStringAsStringPiece(),
         headers.getMethodString(), accessKey, ressource, action, payload);

    return std::string(accessKey);
}


KeyInfoView extractKeyInfoView(const std::string_view key)
{
    CRData rdata(key.data(), key.size());
    unsigned char version;
    if(!rdata.getUChar(&version))
    {
        assert(false);
        XLOGF(ERR, "Corrupted key {}. Version not found", key);
        return {.key=key, .version=0, .bucketId=0};
    }

    int64_t bucketId;
    if(!rdata.getVarInt(&bucketId))
    {
        assert(false);
        XLOGF(ERR, "Corrupted key {}. Bucket id not found", key);
        return {.key=key, .version=0, .bucketId=0};
    }

    if(version==0)
        return {.key=std::string_view(rdata.getCurrDataPtr(), rdata.getLeft()), .version=0, .bucketId=bucketId};

    assert(version==1);
    if(version!=1)
    {
        XLOGF(ERR, "Corrupted key {}. Unknown version {}", key, version);
        return {.key=key, .version=0, .bucketId=0};
    }

    int64_t objectVersion;
    if(!rdata.getVarInt(&objectVersion))
    {
        assert(false);
        XLOGF(ERR, "Corrupted key {}. Object version not found", key);
        return {.key=key, .version=0, .bucketId=0};
    }

    return {.key=std::string_view(rdata.getCurrDataPtr(), rdata.getLeft()), .version=objectVersion, .bucketId=bucketId};
}

KeyInfo extractKeyInfo(const std::string_view key)
{
    auto tmp = extractKeyInfoView(key);
    return {.key=std::string(tmp.key), .version=tmp.version, .bucketId=tmp.bucketId};
}

int	mdb_cmp_s3key(const MDB_val *a, const MDB_val *b)
{
    std::string_view keyA(reinterpret_cast<const char*>(a->mv_data), a->mv_size);
    std::string_view keyB(reinterpret_cast<const char*>(b->mv_data), b->mv_size);

    const auto keyInfoA = extractKeyInfoView(keyA);
    const auto keyInfoB = extractKeyInfoView(keyB);

    if(keyInfoA.bucketId != keyInfoB.bucketId)
        return keyInfoA.bucketId < keyInfoB.bucketId ? 1 : -1;

    int cmp = keyInfoA.key.compare(keyInfoB.key);
    if(cmp!=0)
        return cmp;

    if(keyInfoA.version<keyInfoB.version)
        return 1;
    else if(keyInfoA.version>keyInfoB.version)
        return -1;
    else
        return 0;
}

std::string make_key(const std::string_view key, const int64_t bucketId, const int64_t version)
{
    CWData wdata;
    wdata.addChar(version==0 ? 0 : 1);
    wdata.addVarInt(bucketId);
    if(version!=0)
        wdata.addVarInt(version);
    wdata.addBuffer(key.data(), key.size());
    return wdata.releaseData();
}

std::string make_key(const KeyInfo& keyInfo)
{
    return make_key(keyInfo.key, keyInfo.bucketId, keyInfo.version);
}

std::string s3key_common_prefix(const std::string_view key)
{
    const auto keyInfo = extractKeyInfoView(key);
    return make_key(keyInfo.key, keyInfo.bucketId, 0);
}

size_t s3key_common_prefix_hash(const std::string_view key)
{
    const auto keyInfo = extractKeyInfoView(key);
    return folly::hash::hash_combine(keyInfo.bucketId, keyInfo.key);
}

static void multiPartUploadXmlElementStart(void *userData,
                                               const XML_Char *name,
                                               const XML_Char **atts)
{
    auto multiPartData = reinterpret_cast<S3Handler::MultiPartUploadData*>(userData);

    switch(multiPartData->parseState)
    {
    case S3Handler::MultiPartUploadData::ParseState::Init:
        {
            if(strcmp(name, "CompleteMultipartUpload")==0)
                multiPartData->parseState = S3Handler::MultiPartUploadData::ParseState::InRoot;
            else
                multiPartData->parseState = S3Handler::MultiPartUploadData::ParseState::Unknown;
        } break;
    case S3Handler::MultiPartUploadData::ParseState::InRoot:
        {
            if(strcmp(name, "Part")==0)
            {
                multiPartData->parseState = S3Handler::MultiPartUploadData::ParseState::InPart;
                multiPartData->parts.push_back(S3Handler::MultiPartUploadData::PartData());
            }
            else
            {
                multiPartData->parseState = S3Handler::MultiPartUploadData::ParseState::Unknown;
            }
        } break;
    case S3Handler::MultiPartUploadData::ParseState::InPart:
        {
            if(strcmp(name, "PartNumber")==0)
                multiPartData->parseState = S3Handler::MultiPartUploadData::ParseState::InPartNumber;
            else if(strcmp(name, "ETag")==0)
                multiPartData->parseState = S3Handler::MultiPartUploadData::ParseState::InEtag;
            else
                multiPartData->parseState = S3Handler::MultiPartUploadData::ParseState::InUnknownPartAttr;
        } break;
    default:
        break;
    }
}

static void multiPartUploadXmlElementEnd(void *userData,
                                               const XML_Char *name)
{
    auto multiPartData = reinterpret_cast<S3Handler::MultiPartUploadData*>(userData);

    switch(multiPartData->parseState)
    {
    case S3Handler::MultiPartUploadData::ParseState::InRoot:
        {
            if(strcmp(name, "CompleteMultipartUpload")==0)
                multiPartData->parseState = S3Handler::MultiPartUploadData::ParseState::Finished;
            else
                multiPartData->parseState = S3Handler::MultiPartUploadData::ParseState::Unknown;
        } break;
    case S3Handler::MultiPartUploadData::ParseState::InPart:
        {
            if(strcmp(name, "Part")==0)
                multiPartData->parseState = S3Handler::MultiPartUploadData::ParseState::InRoot;
            else
                multiPartData->parseState = S3Handler::MultiPartUploadData::ParseState::Unknown;
        } break;
    case S3Handler::MultiPartUploadData::ParseState::InPartNumber:
    case S3Handler::MultiPartUploadData::ParseState::InEtag:
    case S3Handler::MultiPartUploadData::ParseState::InUnknownPartAttr:
        {
            multiPartData->parseState = S3Handler::MultiPartUploadData::ParseState::InPart;
        } break;
    default:
        break;
    }
}

static void multiPartUploadXmlCharData(void *userData,
                                               const XML_Char *s, int len)
{
    auto multiPartData = reinterpret_cast<S3Handler::MultiPartUploadData*>(userData);

    if(multiPartData->parseState == S3Handler::MultiPartUploadData::ParseState::InPartNumber &&
        !multiPartData->parts.empty())
    {
        std::string data(s, len);
        int partNum = 0;
        try
        {
            partNum = std::stoi(data);
        }
        catch(std::invalid_argument&)
        {}
        catch(std::out_of_range&)
        {}

        if(multiPartData->parts.size()>1)
        {
            auto& prevPart = multiPartData->parts[multiPartData->parts.size()-2];
            if(partNum<=prevPart.partNumber)
                multiPartData->parseState = S3Handler::MultiPartUploadData::ParseState::InvalidPartOrder;
        }

        multiPartData->parts.back().partNumber = partNum;
    }
    else if(multiPartData->parseState == S3Handler::MultiPartUploadData::ParseState::InEtag &&
        !multiPartData->parts.empty())
    {
        std::string data(s, len);
        multiPartData->parts.back().etag += data;
    }
}

static void deleteObjectsXmlElementStart(void *userData,
                                                const XML_Char *name,
                                                const XML_Char **atts)
{
    auto deleteObjectsData = reinterpret_cast<S3Handler::DeleteObjectsData*>(userData);

    switch(deleteObjectsData->parseState)
    {
    case S3Handler::DeleteObjectsData::ParseState::Init:
        {
            if(strcmp(name, "Delete")==0)
                deleteObjectsData->parseState = S3Handler::DeleteObjectsData::ParseState::InRoot;
            else
                deleteObjectsData->parseState = S3Handler::DeleteObjectsData::ParseState::Unknown;
        } break;
    case S3Handler::DeleteObjectsData::ParseState::InRoot:
    {
        if(strcmp(name, "Object")==0)
        {
            deleteObjectsData->parseState = S3Handler::DeleteObjectsData::ParseState::InObject;
            deleteObjectsData->objects.push_back({});
        }
        else if(strcmp(name, "Quiet") == 0)
            deleteObjectsData->parseState = S3Handler::DeleteObjectsData::ParseState::InQuiet;
        else
            deleteObjectsData->parseState = S3Handler::DeleteObjectsData::ParseState::Unknown;
    } break;
    case S3Handler::DeleteObjectsData::ParseState::InObject:
    {
        if(strcmp(name, "ETag") == 0)
            deleteObjectsData->parseState = S3Handler::DeleteObjectsData::ParseState::InEtag;
        else if(strcmp(name, "Key") == 0)
            deleteObjectsData->parseState = S3Handler::DeleteObjectsData::ParseState::InKey;
        else if(strcmp(name, "LastModifiedTime") == 0)
            deleteObjectsData->parseState = S3Handler::DeleteObjectsData::ParseState::InLastModified;
        else if(strcmp(name, "Size") == 0)
            deleteObjectsData->parseState = S3Handler::DeleteObjectsData::ParseState::InSize;
        else if(strcmp(name, "VersionId") == 0)
            deleteObjectsData->parseState = S3Handler::DeleteObjectsData::ParseState::InVersionId;
        else
            deleteObjectsData->parseState = S3Handler::DeleteObjectsData::ParseState::InUnknownObjectAttr;
    } break;
    default:
        break;
    }
}

static void deleteObjectsXmlElementEnd(void *userData,
                                               const XML_Char *name)
{
    auto deleteObjectsData = reinterpret_cast<S3Handler::DeleteObjectsData*>(userData);

    switch(deleteObjectsData->parseState)
    {
    case S3Handler::DeleteObjectsData::ParseState::InRoot:
        {
            if(strcmp(name, "Delete")==0)
                deleteObjectsData->parseState = S3Handler::DeleteObjectsData::ParseState::Finished;
            else
                deleteObjectsData->parseState = S3Handler::DeleteObjectsData::ParseState::Unknown;
        } break;
    case S3Handler::DeleteObjectsData::ParseState::InObject:
        {
            if(strcmp(name, "Object")==0)
                deleteObjectsData->parseState = S3Handler::DeleteObjectsData::ParseState::InRoot;
            else
                deleteObjectsData->parseState = S3Handler::DeleteObjectsData::ParseState::Unknown;
        } break;
    case S3Handler::DeleteObjectsData::ParseState::InEtag:
    case S3Handler::DeleteObjectsData::ParseState::InKey:
    case S3Handler::DeleteObjectsData::ParseState::InLastModified:
    case S3Handler::DeleteObjectsData::ParseState::InSize:
    case S3Handler::DeleteObjectsData::ParseState::InVersionId:
    case S3Handler::DeleteObjectsData::ParseState::InUnknownObjectAttr:
        {
            deleteObjectsData->parseState = S3Handler::DeleteObjectsData::ParseState::InObject;
        } break;
    case S3Handler::DeleteObjectsData::ParseState::InQuiet:
        {
            if(strcmp(name, "Quiet") == 0)
                deleteObjectsData->parseState = S3Handler::DeleteObjectsData::ParseState::InRoot;
            else
                deleteObjectsData->parseState = S3Handler::DeleteObjectsData::ParseState::Unknown;
        }
    default:
        break;
    }
}

static void deleteObjectsXmlCharData(void *userData,
                                               const XML_Char *s, int len)
{
    auto deleteObjectsData = reinterpret_cast<S3Handler::DeleteObjectsData*>(userData);

    std::string_view data(s, len);

    switch(deleteObjectsData->parseState)
    {
        case S3Handler::DeleteObjectsData::ParseState::InKey:
        {
            auto& obj = deleteObjectsData->objects.back();
            obj.key = std::string(data);
        } break;
        case S3Handler::DeleteObjectsData::ParseState::InEtag:
        {
            auto& obj = deleteObjectsData->objects.back();
            obj.etag = std::string(data);
        } break;
        case S3Handler::DeleteObjectsData::ParseState::InLastModified:
        {
            auto& obj = deleteObjectsData->objects.back();
            try
            {
                obj.lastModified = 0; //TODO //folly::parseUnixTimestamp(data);
            }
            catch(const std::exception& ex)
            {
                XLOGF(ERR, "Invalid last modified time {}: {}", data, folly::exceptionStr(ex));
                deleteObjectsData->parseState = S3Handler::DeleteObjectsData::ParseState::Invalid;
            }
        } break;
        case S3Handler::DeleteObjectsData::ParseState::InSize:
        {
            auto& obj = deleteObjectsData->objects.back();
            try
            {
                obj.size = std::stoll(data.data(), nullptr, 10);
            }
            catch(const std::exception& ex)
            {
                XLOGF(ERR, "Invalid size {}: {}", data, folly::exceptionStr(ex));
                deleteObjectsData->parseState = S3Handler::DeleteObjectsData::ParseState::Invalid;
            }
        } break;
        case S3Handler::DeleteObjectsData::ParseState::InVersionId:
        {
            auto& obj = deleteObjectsData->objects.back();
            try
            {
                obj.versionId = std::stoll(data.data(), nullptr, 10);
            }
            catch(const std::exception& ex)
            {
                XLOGF(ERR, "Invalid version id {}: {}", data, folly::exceptionStr(ex));
                deleteObjectsData->parseState = S3Handler::DeleteObjectsData::ParseState::Invalid;
            }
        } break;
        case S3Handler::DeleteObjectsData::ParseState::InQuiet:
        {
            if(data == "true")
            {
                deleteObjectsData->quiet = true;
            }
            else
            {
                XLOGF(ERR, "Invalid quiet parameter for delete: ", data);
                deleteObjectsData->parseState = S3Handler::DeleteObjectsData::ParseState::Invalid;
            }
        } break;
        default:
            break;
    }
}

std::optional<std::string> S3Handler::initPayloadHash(proxygen::HTTPMessage& message)
{
    const auto payload = message.getHeaders().getSingleOrEmpty("x-amz-content-sha256");

    if(payload == unsigned_payload)
        return payload;

    std::string payloadBin;
    if(!folly::unhexlify(payload, payloadBin))
    {
        XLOGF(INFO, "Invalid payload hash: {}", payload);
        ResponseBuilder(downstream_)
            .status(400, "Bad request")
            .body("Invalid payload hash")
            .sendWithEOM();
        return std::nullopt;
    }

    if(!payload.empty())
    {
        payloadHash = std::make_unique<PayloadHash>();
        payloadHash->method = PayloadHash::Method::Sha256;
        payloadHash->expectedHash = payloadBin;
        if(!payloadHash->evpMdCtx.init(EVP_sha256()))
        {
            XLOGF(INFO, "Failed to init sha256 context");
            ResponseBuilder(downstream_)
                .status(500, "Internal error")
                .body("Failed to init sha256 context")
                .sendWithEOM();
            return std::nullopt;
        }
        if(!payloadHash->checkSize())
        {
            XLOGF(INFO, "Payload hash size mismatch");
            ResponseBuilder(downstream_)
                .status(400, "Bad request")
                .body("Payload hash size mismatch")
                .sendWithEOM();
            return std::nullopt;
        }
    }

    return payload;
}

/**
 * Handles requests by serving the file named in path.  Only supports GET.
 * reads happen in a CPU thread pool since read(2) is blocking.
 * If egress pauses, file reading is also paused.
 */

void S3Handler::onRequest(std::unique_ptr<HTTPMessage> headers) noexcept
{
    headers->dumpMessage(static_cast<int>(folly::LogLevel::DBG0));

    if (headers->getMethod() != HTTPMethod::PUT &&
        headers->getMethod() != HTTPMethod::GET &&
        headers->getMethod() != HTTPMethod::HEAD &&
        headers->getMethod() != HTTPMethod::DELETE &&
        headers->getMethod() != HTTPMethod::POST)
    {
        ResponseBuilder(downstream_)
            .status(400, "Bad method")
            .body("Only GET/PUT/HEAD/DELETE/POST is supported")
            .sendWithEOM();
        return;
    }

    if (headers->getMethod() == HTTPMethod::GET || headers->getMethod() == HTTPMethod::HEAD)
    {
        request_type = headers->getMethod() == HTTPMethod::GET ? RequestType::GetObject : RequestType::HeadObject;

        const auto header_path = headers->getPathAsStringPiece();

        if(header_path.empty())
        {
            ResponseBuilder(downstream_)
                        .status(500, "Internal error")
                        .body("Path is empty")
                        .sendWithEOM();
            return;
        }

        running = true;

        const auto nextSlash = header_path.find('/', 1);
        if(nextSlash==std::string::npos || nextSlash == header_path.size()-1)
        {
            const auto bucketName = header_path.subpiece(1, nextSlash == std::string::npos ? std::string::npos : nextSlash - 1);
            if(!checkSig(*headers, "", fmt::format("arn:aws:s3:::{}", bucketName), "s3:ListBucket", false))
            {
                XLOGF(INFO, "Unauthorized list bucket: {}", bucketName);
                ResponseBuilder(downstream_)
                    .status(401, "Unauthorized")
                    .body("Verifying request authorization failed")
                    .sendWithEOM();
                return;
            }
            XLOGF(INFO, "List bucket {}", bucketName);
            listObjects(*headers, std::string(bucketName));
            return;
        }   

        const auto resource = fmt::format("arn:aws:s3:::{}", header_path.subpiece(1));
        const auto action = headers->getMethod() == HTTPMethod::GET ? "s3:GetObject" : "s3:HeadObject";

        auto accessKey = checkSig(*headers, "", resource, action, true);

        if(!accessKey)
        {
            XLOGF(INFO, "Get object unauthorized: {}", header_path);
             ResponseBuilder(downstream_)
                    .status(401, "Unauthorized")
                    .body("Verifying request authorization failed")
                    .sendWithEOM();
                return;
        }

        if(header_path.find(c_commit_uuid)!=std::string::npos)
        {
            XLOGF(INFO, "Getting commit object");
            getCommitObject(*headers);
            return;
        }

        if(!setKeyInfoFromPath(header_path))
            return;
        
        XLOGF(INFO, "Get object {} range {}", header_path, headers->getHeaders().getSingleOrEmpty(proxygen::HTTP_HEADER_RANGE));
        getObject(*headers, *accessKey);
        return;
    }
    else if (headers->getMethod() == HTTPMethod::PUT)
    {
        std::string cl = headers->getHeaders().getSingleOrEmpty(
            proxygen::HTTP_HEADER_CONTENT_LENGTH);
        if (cl.empty())
        {
            ResponseBuilder(downstream_)
                .status(500, "Internal error")
                .body("Content-Length header not set")
                .sendWithEOM();
            return;
        }
        auto remaining = std::atoll(cl.c_str());
        put_remaining = remaining;
        const auto path = headers->getPathAsStringPiece();

        std::string xid;
        if(!headers->getQueryStringAsStringPiece().empty())
        {
            xid = headers->getDecodedQueryParam("x-id");
        }

        const auto createBucket = !path.empty() && path.find_first_of('/', 1)==std::string::npos && (xid.empty() || xid == "CreateBucket");

        if(createBucket)
            keyInfo.key = path.substr(1);
        else if(!setKeyInfoFromPath(path))
            return;

        const auto resource = fmt::format("arn:aws:s3:::{}", path.substr(1));
        const auto payloadOpt = initPayloadHash(*headers);
        if(!payloadOpt)
            return;
        const auto payload = *payloadOpt;

        if(!createBucket && !headers->getQueryStringAsStringPiece().empty() && (xid.empty() || xid == "UploadPart"))
        {
            int partNumber = 0;
            const auto& queryParams = headers->getQueryParams();
            const auto partNumberIt = queryParams.find("partNumber");
            if(partNumberIt != queryParams.end())
            {
                try
                {
                    partNumber = std::stoi(partNumberIt->second);
                }
                catch(std::invalid_argument&)
                {
                    partNumber = 0;
                }
                catch(std::out_of_range&)
                {
                    partNumber=0;
                }
            }
            const auto uploadIdIt = queryParams.find("uploadId");
            std::string uploadIdHex, uploadIdStr;
            std::string uploadVerIdHex, uploadVerId;
            if(uploadIdIt != queryParams.end() )
            {
                if(!folly::split("-", uploadIdIt->second, uploadIdHex, uploadVerIdHex)
                 || !folly::unhexlify(uploadIdHex, uploadIdStr)
                 || !folly::unhexlify(uploadVerIdHex, uploadVerId) )
                 uploadIdStr.clear();
            }

            auto uploadId = sfs.decrypt_id(uploadIdStr);

            if(uploadId<0 || uploadVerId.empty())
            {
                XLOGF(INFO, "UploadId param not set in multi-part upload of {}", path);
                ResponseBuilder(downstream_)
                    .status(500, "Internal error")
                    .body("uploadId parameter not set")
                    .sendWithEOM();
                return;
            }

            if(partNumber<=0 || partNumber>10000)
            {
                XLOGF(INFO, "PartNumber {} out of range in multi-part upload of {}", partNumber, path);
                ResponseBuilder(downstream_)
                    .status(500, "Internal error")
                    .body("partNumber parameter out of range")
                    .sendWithEOM();
                return;
            }

            XLOGF(DBG0, "PutObjectPart {} part {} uploadId {} length {}", path, partNumber, uploadId, remaining);

            const auto action = "s3:PutObjectPart";

            if(!checkSig(*headers, payload, resource, action, true))
            {
                XLOGF(INFO, "Unauthorized putObjectPart: {}", path);
                ResponseBuilder(downstream_)
                    .status(401, "Unauthorized")
                    .body("Verifying request authorization failed")
                    .sendWithEOM();
                return;
            }

            putObjectPart(*headers, partNumber, uploadId, uploadVerId);
            return;
        }

        const auto action = createBucket ? "s3:CreateBucket" : "s3:PutObject";

        if(!checkSig(*headers, payload, resource, action, true))
        {
            XLOGF(INFO, "Unauthorized putObject: {}", path);
            ResponseBuilder(downstream_)
                .status(401, "Unauthorized")
                .body("Verifying request authorization failed")
                .sendWithEOM();
            return;
        }

        if(createBucket)
        {
            if(!xid.empty() && xid != "CreateBucket")
            {
                XLOGF(INFO, "Invalid x-id for create bucket: {}", xid);
                ResponseBuilder(downstream_)
                    .status(400, "Bad request")
                    .body("Invalid x-id for create bucket")
                    .sendWithEOM();
                return;
            }

            request_type = RequestType::CreateBucket;
            return;
        }

        if(!xid.empty() && xid != "PutObject")
        {
            XLOGF(INFO, "Invalid x-id for put object: {}", xid);
            ResponseBuilder(downstream_)
                .status(400, "Bad request")
                .body("Invalid x-id for put object")
                .sendWithEOM();
            return;
        }

        if(path.find(c_commit_uuid)!=std::string::npos)
        {
            XLOGF(DBG0, "PutObject {} COMMIT", path);

            commit(*headers);
            // Don't handle EOM
            request_type = RequestType::HeadObject;
            return;
        }        

        if(FLAGS_with_stop_command &&
            path.find(c_stop_uuid)!=std::string::npos)
        {
            XLOGF(DBG0, "PutObject {} STOP", path);
            folly::getGlobalCPUExecutor()->add([]{
                stopServer();
            });
            ResponseBuilder(downstream_)
                    .status(200, "OK")
                    .body("Server is stopping")
                    .sendWithEOM();
            // Don't handle EOM
            request_type = RequestType::HeadObject;
            return;
        }

        XLOGF(DBG0, "PutObject {} length {}", path, remaining);

        putObject(*headers);
        return;
    }
    else if(headers->getMethod() == HTTPMethod::DELETE)
    {
        const auto path = headers->getPathAsStringPiece();
        const auto isDeleteBucket = !path.empty() && path.find_first_of('/', 1)==std::string::npos;

        if(isDeleteBucket)
            keyInfo.key = path.substr(1);
        else if(!setKeyInfoFromPath(headers->getPathAsStringPiece()))
            return;

        request_type = RequestType::DeleteObject;

        const auto resource = fmt::format("arn:aws:s3:::{}", path.substr(1));
        const auto action = isDeleteBucket ? "s3:DeleteBucket" : "s3:DeleteObject";

        //TODO: Double-check that pre-signed delete is possible
        if(!checkSig(*headers, "", resource, action, true))
        {
            XLOGF(INFO, "Unauthorized delete object: {}", path);
            ResponseBuilder(downstream_)
                .status(401, "Unauthorized")
                .body("Verifying request authorization failed")
                .sendWithEOM();
            return;
        }

        XLOGF(INFO, "Delete {}", path);

        if(isDeleteBucket)
            deleteBucket(*headers);
        else
            deleteObject(*headers);
    }
    else if(headers->getMethod() == HTTPMethod::POST)
    {
        if(headers->getQueryStringAsStringPiece() == "delete")
        {
            std::string cl = headers->getHeaders().getSingleOrEmpty(
                    proxygen::HTTP_HEADER_CONTENT_LENGTH);
            if (cl.empty())
            {
                ResponseBuilder(downstream_)
                    .status(500, "Internal error")
                    .body("Content-Length header not set")
                    .sendWithEOM();
                return;
            }

            if(headers->getPathAsStringPiece().size()<=1)
            {
                ResponseBuilder(downstream_)
                    .status(500, "Internal error")
                    .body("Bucket not found in path")
                    .sendWithEOM();
                return;
            }

            const auto resource = fmt::format("arn:aws:s3:::{}", headers->getPathAsStringPiece().substr(1));
            const auto payloadOpt = initPayloadHash(*headers);
            if(!payloadOpt)
            {
                ResponseBuilder(downstream_)
                    .status(500, "Internal error")
                    .body("No payload hash present")
                    .sendWithEOM();
                return;
            }
            const auto payload = *payloadOpt;

            const auto bucketName = headers->getPathAsStringPiece().substr(1);

            const auto accessKey = checkSig(*headers, payload, std::string_view(), std::string_view(), true);
            if(!accessKey)
            {
                XLOGF(INFO, "Unauthorized delete object: {}", headers->getPathAsStringPiece());
                ResponseBuilder(downstream_)
                    .status(401, "Unauthorized")
                    .body("Verifying request authorization failed")
                    .sendWithEOM();
                return;
            }

            const auto bucketId = buckets::getBucket(bucketName);
            if(!bucketId)
            {
                XLOGF(INFO, "Bucket {} not found", bucketName);
                ResponseBuilder(downstream_)
                            .status(404, "Not found")
                            .body("Bucket not found")
                            .sendWithEOM();
                return;
            }

            keyInfo.bucketId = *bucketId;

            if(!evpMdCtx.init(EVP_md5()) )
            {
                ResponseBuilder(downstream_)
                    .status(500, "Internal error")
                    .body("Could not init MD5 content hash")
                    .sendWithEOM();
                return;
            }

            request_type = RequestType::DeleteObjects;
            put_remaining = std::atoll(cl.c_str());

            xmlBody.init();
            if(xmlBody.parser == nullptr)
            {
                ResponseBuilder(downstream_)
                    .status(500, "Internal error")
                    .body("Could not init XML parser")
                    .sendWithEOM();
                return;
            }

            deleteObjectsData = std::make_unique<DeleteObjectsData>();
            deleteObjectsData->accessKey = *accessKey;
            XML_SetUserData(xmlBody.parser, deleteObjectsData.get());

            XML_SetElementHandler(xmlBody.parser, deleteObjectsXmlElementStart, deleteObjectsXmlElementEnd);
            XML_SetCharacterDataHandler(xmlBody.parser, deleteObjectsXmlCharData);

            XLOGF(INFO, "Delete objects in bucket {}", bucketName);
            return;
        }

        if(!setKeyInfoFromPath(headers->getPathAsStringPiece()))
            return;

        const std::string uploadsStr = "uploads";
        
        if(headers->getQueryStringAsStringPiece() == uploadsStr ||
            headers->hasQueryParam("uploads"))
        {
            const auto resource = fmt::format("arn:aws:s3:::{}", headers->getPathAsStringPiece().substr(1));
            const auto action = "s3:CreateMultipartUpload";

            if(!checkSig(*headers, "", resource, action, true))
            {
                XLOGF(INFO, "Unauthorized create multi-part upload: {}", headers->getPathAsStringPiece());
                ResponseBuilder(downstream_)
                    .status(401, "Unauthorized")
                    .body("Verifying request authorization failed")
                    .sendWithEOM();
                return;
            }

            XLOGF(INFO, "Multi-part upload of {}", keyInfo.key);
            createMultipartUpload(*headers);
            return;
        }
        else
        {
            const auto payloadOpt = initPayloadHash(*headers);
            if(!payloadOpt)
                return;
            const auto payload = *payloadOpt;

            std::string uploadIdHex, uploadVerIdHex;
            std::string uploadIdStr, uploadVerId;
            const auto uploadIdIt = headers->getQueryParams().find("uploadId");
            if(uploadIdIt != headers->getQueryParams().end())
            {
                if(!folly::split("-", uploadIdIt->second, uploadIdHex, uploadVerIdHex)
                 || !folly::unhexlify(uploadIdHex, uploadIdStr)
                 || !folly::unhexlify(uploadVerIdHex, uploadVerId) )
                    uploadIdStr.clear();
            }

            const auto uploadId = sfs.decrypt_id(uploadIdStr);

            if(uploadId<0)
            {
                ResponseBuilder(downstream_)
                    .status(500, "Internal error")
                    .body("UploadId parameter not found")
                    .sendWithEOM();
                return;
            }

            std::string cl = headers->getHeaders().getSingleOrEmpty(
                    proxygen::HTTP_HEADER_CONTENT_LENGTH);
            if (cl.empty())
            {
                ResponseBuilder(downstream_)
                    .status(500, "Internal error")
                    .body("Content-Length header not set")
                    .sendWithEOM();
                return;
            }
            put_remaining = std::atoll(cl.c_str());
            request_type = RequestType::CompleteMultipartUpload;
            xmlBody.init();
            if(xmlBody.parser == nullptr)
            {
                ResponseBuilder(downstream_)
                    .status(500, "Internal error")
                    .body("Could not init XML parser")
                    .sendWithEOM();
                return;
            }
            multiPartUploadData = std::make_unique<MultiPartUploadData>();
            multiPartUploadData->uploadId = uploadId;
            multiPartUploadData->verId = uploadVerId;

            XML_SetUserData(xmlBody.parser, multiPartUploadData.get());

            XML_SetElementHandler(xmlBody.parser, multiPartUploadXmlElementStart, multiPartUploadXmlElementEnd);
            XML_SetCharacterDataHandler(xmlBody.parser, multiPartUploadXmlCharData);

            XLOGF(INFO, "Complete multi-part upload of {} uploadId {}", keyInfo.key, uploadId);
        }
    }
}

bool S3Handler::setKeyInfoFromPath(const std::string_view path)
{
    if(path.empty())
    {
        XLOGF(INFO, "Path empty when getting key info");
        ResponseBuilder(downstream_)
                    .status(500, "Internal error")
                    .body("Path is empty")
                    .sendWithEOM();
        return false;
    }

    const auto bucketEnd = path.find_first_of('/', 1);

    if(bucketEnd == std::string::npos)
    {
        XLOGF(INFO, "Could not find bucket in path {}", path);
        ResponseBuilder(downstream_)
                    .status(500, "Internal error")
                    .body("Could not find bucket in path")
                    .sendWithEOM();
        return false;
    }

    const auto bucketName = path.substr(1, bucketEnd-1);
    const auto keyStr = path.substr(bucketEnd+1);

    const auto bucketId = buckets::getBucket(bucketName);
    if(!bucketId)
    {
        XLOGF(INFO, "Bucket of {} not found", path);
        ResponseBuilder(downstream_)
                    .status(404, "Not found")
                    .body("Bucket not found")
                    .sendWithEOM();
        return false;
    }

    keyInfo.bucketId = *bucketId;
    keyInfo.key = keyStr;
    keyInfo.version = 0;

    return true;
}

void S3Handler::listObjects(proxygen::HTTPMessage& headers, const std::string& bucket)
{
    auto bucketId = buckets::getBucket(bucket);
    if(!bucketId)
    {
        ResponseBuilder(self->downstream_)
                            .status(404, "Not found")
                            .body(fmt::format("Bucket not found"))
                            .sendWithEOM();
        return;
    }

    keyInfo.bucketId = *bucketId;

    const auto listType = headers.getQueryParam("list-type");
    if(listType=="2")
    {
        listObjectsV2(headers, bucket, *bucketId);
        return;
    }
    else if(!listType.empty())
    {
        ResponseBuilder(self->downstream_)
                            .status(500, "Internal error")
                            .body(fmt::format("Unknown list type"))
                            .sendWithEOM();
        return;
    }

    request_type = RequestType::ListObjects;
    const auto marker = headers.getDecodedQueryParam("marker");
    const auto maxKeys = headers.getIntQueryParam("max-keys", 1000);
    const auto prefix = headers.hasQueryParam("prefix") ? std::make_optional(headers.getDecodedQueryParam("prefix")) : std::nullopt;
    const auto delimiter = headers.getDecodedQueryParam("delimiter");

    if(delimiter.size()>1)
    {
        ResponseBuilder(self->downstream_)
                            .status(500, "Internal error")
                            .body(fmt::format("Delimiter has more than one char"))
                            .sendWithEOM();
        return;
    }

    auto evb = folly::EventBaseManager::get()->getEventBase();

    folly::getGlobalCPUExecutor()->add(
    [self = self, evb, marker, maxKeys, prefix, delimiter, bucket, bucketId]()
    {
        self->listObjects(evb, self, marker, std::max(0, std::min(10000, maxKeys)), prefix, std::nullopt, delimiter, *bucketId, false, bucket);
    });
}

void S3Handler::listObjectsV2(proxygen::HTTPMessage& headers, const std::string& bucket, const int64_t bucketId)
{
    request_type = RequestType::ListObjects;
    const auto continuationToken = headers.getDecodedQueryParam("continuation-token");
    const auto maxKeys = headers.getIntQueryParam("max-keys", 1000);
    const auto prefix = headers.hasQueryParam("prefix") ? std::make_optional(headers.getDecodedQueryParam("prefix")) : std::nullopt;
    const auto delimiter = headers.getDecodedQueryParam("delimiter");
    const auto startAfter = headers.hasQueryParam("start-after") ? std::make_optional(headers.getDecodedQueryParam("start-after")) : std::nullopt;

    if(delimiter.size()>1)
    {
        ResponseBuilder(self->downstream_)
                            .status(500, "Internal error")
                            .body(fmt::format("Delimiter has more than one char"))
                            .sendWithEOM();
        return;
    }

    std::string marker;
    if(!folly::unhexlify(continuationToken, marker))
    {
        ResponseBuilder(self->downstream_)
                            .status(500, "Internal error")
                            .body(fmt::format("Cannot decode continuation token"))
                            .sendWithEOM();
        return;
    }

    auto evb = folly::EventBaseManager::get()->getEventBase();

    folly::getGlobalCPUExecutor()->add(
    [self = self, evb, marker, maxKeys, prefix, startAfter, delimiter, bucket, bucketId]()
    {
        self->listObjects(evb, self, marker, std::max(0, std::min(10000, maxKeys)), prefix, startAfter, delimiter, bucketId, true, bucket);
    });
}

void S3Handler::getCommitObject(proxygen::HTTPMessage& headers)
{
    const auto runtime_id = sfs.get_runtime_id();
    if(request_type==RequestType::HeadObject)
    {
        ResponseBuilder(self->downstream_).status(200, "OK").header(proxygen::HTTP_HEADER_CONTENT_LENGTH, std::to_string(runtime_id.size())).sendWithEOM();
        return;
    }

    ResponseBuilder(self->downstream_)
                        .status(200, "OK")
                        .header(proxygen::HTTP_HEADER_CONTENT_LENGTH, std::to_string(runtime_id.size()))
                        .header(proxygen::HTTP_HEADER_CONTENT_TYPE, "binary/octet-stream")
                        .body(fmt::format("{}", sfs.get_runtime_id()))
                        .sendWithEOM();
}

bool S3Handler::parseMultipartInfo(const std::string& md5sum, int64_t& totalLen, std::unique_ptr<MultiPartDownloadData>& multiPartDownloadData)
{
#ifdef ALLOW_LEGACY_MD5SUM
    if(md5sum.size()==MD5_DIGEST_LENGTH || md5sum.empty())
    {
        return true;
    }
#endif

    CRData rdata(md5sum.data(), md5sum.size());
    char itype;
    if(!rdata.getChar(&itype))
        return false;

    if(itype==metadata_object || itype==metadata_tombstone)
        return true;

    assert(itype == metadata_multipart_object);

    if(itype != metadata_multipart_object)
        return false;
    
    multiPartDownloadData = std::make_unique<MultiPartDownloadData>();
    if(rdata.getLeft()>=MD5_DIGEST_LENGTH)
    {
        multiPartDownloadData->etag.assign(rdata.getCurrDataPtr(), MD5_DIGEST_LENGTH);
        rdata.incrementPtr(MD5_DIGEST_LENGTH);
    }

    if(!rdata.getVarInt(&multiPartDownloadData->uploadId))
        return false;

    if(!rdata.getVarInt(&multiPartDownloadData->numParts))
        return false;

    if(multiPartDownloadData->numParts<0)
        return false;

    totalLen = 0;
    int64_t numParts = 0;
    int64_t size, start, len;
    while(rdata.getVarInt(&size) &&
        rdata.getVarInt(&start) &&
        rdata.getVarInt(&len))
    {
        numParts += len;
        totalLen += len*size;
        MultiPartDownloadData::PartExt ext{.size=size, .start=static_cast<int>(start), .len=static_cast<int>(len)};
        multiPartDownloadData->exts.emplace_back(std::move(ext));
    }

    return true;
}

std::string S3Handler::getEtag(const std::string& md5sum)
{
#ifdef ALLOW_LEGACY_MD5SUM
    if(md5sum.size()==MD5_DIGEST_LENGTH || md5sum.empty())
    {
        return md5sum;
    }
#endif

    if(md5sum.empty())
        return std::string();

    CRData rdata(md5sum.data(), md5sum.size());
    char itype;
    if(!rdata.getChar(&itype) || itype == metadata_tombstone)
        return std::string();

    if(itype != metadata_multipart_object && itype != metadata_object)
    {
        assert(false);
        return std::string();
    }

    std::string etag;

    if(rdata.getLeft()>=MD5_DIGEST_LENGTH)
    {
        etag.assign(rdata.getCurrDataPtr(), MD5_DIGEST_LENGTH);
        rdata.incrementPtr(MD5_DIGEST_LENGTH);
    }

    if(itype == metadata_object)
    {
        return fmt::format("\"{}\"", folly::hexlify(etag));
    }

    int64_t uploadId, numParts;

    if(!rdata.getVarInt(&uploadId))
        return fmt::format("\"{}\"", folly::hexlify(etag));

    if(!rdata.getVarInt(&numParts))
        return fmt::format("\"{}\"", folly::hexlify(etag));

    return fmt::format("\"{}-{}\"", folly::hexlify(etag), numParts);
}

std::string S3Handler::getEtagParsedMultipart(const std::string& md5sum)
{
    if(multiPartDownloadData)
        return fmt::format("\"{}-{}\"", folly::hexlify(multiPartDownloadData->etag), multiPartDownloadData->numParts);
    else
        return fmt::format("\"{}\"", folly::hexlify(md5sum));
}

void S3Handler::commit(proxygen::HTTPMessage& headers)
{
    if(put_remaining>0)
    {
            ResponseBuilder(self->downstream_)
                    .status(500, "Internal error")
                    .body(fmt::format("Body length != 0"))
                    .sendWithEOM();
    }

    auto evb = folly::EventBaseManager::get()->getEventBase();
    folly::getGlobalCPUExecutor()->add(
    [self = this->self, evb]()
    {
        bool b = self->sfs.commit(false, -1, FLAGS_pre_sync_commit);

        evb->runInEventBaseThread([self = self, b]()
                                        {
        if(!b)
        {
            ResponseBuilder(self->downstream_)
                    .status(500, "Internal error")
                    .body(fmt::format("Commit error"))
                    .sendWithEOM();
        }
        else {
            ResponseBuilder(self->downstream_)
                    .status(200, "OK")
                    .sendWithEOM();
        }
                                        });
    });
}

std::pair<int64_t, int64_t> parseRange(const std::string_view range, int64_t fileSize)
{
    if(range.empty())
        return std::make_pair(0, fileSize);

    int64_t start = -1;
    int64_t end = -1;
    if(!range.starts_with("bytes="))
        return std::make_pair(start, end);
    if(range.find(',')!=std::string::npos)
        return std::make_pair(start, end);

    const auto byteRange = range.substr(6);
    const auto dashPos = byteRange.find("-");
    if(dashPos==std::string::npos)
        return std::make_pair(start, end);

    try
    {
        auto startStr = std::string(byteRange.substr(0, dashPos));
        auto endStr = std::string(byteRange.substr(dashPos+1));
        if(startStr.empty() && !endStr.empty())
        {
            start = 0;
            end = fileSize - std::stoll(endStr);
        }
        else if(!startStr.empty() && endStr.empty())
        {
            start =  std::stoll(startStr);
            end = fileSize;
        }
        else
        {
            start = std::stoll(startStr);
            end = std::stoll(endStr) + 1;
        }
    }
    catch(const std::exception&)
    {
        XLOGF(INFO, "Error parsing range header {}", range);
        start = -1;
        return std::make_pair(start, end);
    }
    
    return std::make_pair(start, end);
}

std::string format_last_modified(const int64_t lastModified)
{
    constexpr auto epoch = std::chrono::time_point<std::chrono::system_clock>();
    const auto lastModifiedTp = std::chrono::system_clock::to_time_t(epoch + std::chrono::nanoseconds(lastModified));

    char datebuf[sizeof("2011-10-08T07:07:09Z")];
    std::strftime(datebuf, sizeof(datebuf), "%FT%TZ", std::gmtime(&lastModifiedTp));
    return datebuf;
}

void S3Handler::getObject(proxygen::HTTPMessage& headers, const std::string& accessKey)
{
    auto evb = folly::EventBaseManager::get()->getEventBase();
    folly::getGlobalCPUExecutor()->add(
            [self = self, evb, accessKey, range=headers.getHeaders().getSingleOrEmpty(proxygen::HTTP_HEADER_RANGE)]()
            { 
                unsigned int flags = 0;
                if(self->request_type == RequestType::HeadObject)
                    flags |= SingleFileStorage::ReadMetaOnly;
                
                std::string findPath;

                if(self->withBucketVersioning && self->keyInfo.version==0)
                {
                    flags |= SingleFileStorage::ReadNewest;
                    findPath = make_key(self->keyInfo.key, self->keyInfo.bucketId, std::numeric_limits<int64_t>::max());
                }
                else
                {
                    findPath = make_key(self->keyInfo);
                }

                auto res = self->sfs.read_prepare(findPath, flags);

                if (res.err != 0)
                {
                    XLOGF(INFO, "Object {} err {}", self->keyInfo.key, res.err);
                    evb->runInEventBaseThread([self = self, res, accessKey]()
                                              {

                        if(res.err==ENOENT)
                        {
                            const auto bucketName = buckets::getBucketName(self->keyInfo.bucketId);
                            if(isAuthorized(fmt::format("arn:aws:s3:::{}", bucketName), "s3:ListBucket", accessKey))
                            {
                                ResponseBuilder(self->downstream_)
                                    .status(404, "Not found")
                                    .body(fmt::format("Object not found"))
                                    .sendWithEOM();
                            }
                            else
                            {
                                ResponseBuilder(self->downstream_)
                                    .status(403, "Forbidden")
                                    .body(fmt::format("Access is forbidden"))
                                    .sendWithEOM();
                            }
                        }
                        else if(res.err==ENOTRECOVERABLE)
                        {
                            ResponseBuilder(self->downstream_)
                                .status(500, "Internal error")
                                .body(fmt::format("Storage is dead"))
                                .sendWithEOM();
                        }
                        else
                        {
                            ResponseBuilder(self->downstream_)
                                .status(500, "Internal error")
                                .body(fmt::format("Error code: {}", res.err))
                                .sendWithEOM();
                        }
                                              });
                    return;
                }

                if(!self->parseMultipartInfo(res.md5sum, res.total_len, self->multiPartDownloadData))
                {
                    evb->runInEventBaseThread([self = self]()
                                              {
                                ResponseBuilder(self->downstream_)
                                .status(500, "Internal error")
                                .body(fmt::format("Error parsing md5sum data"))
                                .sendWithEOM();
                                              });
                                            return;
                } 

                if(res.md5sum.size()==1 && res.md5sum[0] == metadata_tombstone)
                {
                    const auto bucketName = buckets::getBucketName(self->keyInfo.bucketId);
                    if(isAuthorized(fmt::format("arn:aws:s3:::{}", bucketName), "s3:ListBucket", accessKey))
                    {
                        evb->runInEventBaseThread([self = self, res]()
                        {
                            ResponseBuilder(self->downstream_)
                                        .status(404, "Not found")
                                        .body(fmt::format("Object not found"))
                                        .sendWithEOM();
                        });
                    }
                    else
                    {
                        evb->runInEventBaseThread([self = self, res]()
                        {
                            ResponseBuilder(self->downstream_)
                                    .status(403, "Forbidden")
                                    .body(fmt::format("Access is forbidden"))
                                    .sendWithEOM();
                        });
                    }
                    return;
                }

                #ifdef ALLOW_LEGACY_MD5SUM
                const auto md5sum = res.md5sum.size() == MD5_DIGEST_LENGTH ? res.md5sum : ((!res.md5sum.empty() && res.md5sum[0] == metadata_object ) ? res.md5sum.substr(1) : "");
                #else
                const auto md5sum = (!res.md5sum.empty() && res.md5sum[0] == metadata_object ) ? res.md5sum.substr(1, MD5_DIGEST_LENGTH) : "";
                #endif

                const auto [rangeStart, rangeEnd] = parseRange(range, res.total_len);

                if(!range.empty() && 
                    (rangeStart<0 || rangeEnd<0 || rangeStart>=rangeEnd || rangeEnd > res.total_len) )
                {
                    evb->runInEventBaseThread([self = self, res]()
                        {
                            ResponseBuilder(self->downstream_)
                                    .status(416, "Range Not Satisfiable")
                                    .sendWithEOM();
                        });
                    return;
                }

                evb->runInEventBaseThread([self = self, hasRange=!range.empty(), rangeStart, rangeEnd, md5sum, last_modified = res.last_modified]()
                                              {
                    auto resp = std::move(ResponseBuilder(self->downstream_).status(hasRange ? 206 : 200, hasRange ? "Partial Content" : "OK")
                        .header(proxygen::HTTP_HEADER_CONTENT_LENGTH, std::to_string(rangeEnd-rangeStart))
                        .header(proxygen::HTTP_HEADER_ETAG, self->getEtagParsedMultipart(md5sum))
                        .header(proxygen::HTTP_HEADER_CONTENT_TYPE, "binary/octet-stream")
                        .header("Last-Modified", format_last_modified(last_modified)));

                    if(self->request_type==RequestType::HeadObject)
                    {
                        XLOGF(DBG0, "Content length {} bytes for readObject HEAD of {}", rangeEnd, self->keyInfo.key);
                        // .send() sets the content length to the body length (which is zero)
                        auto respHeaders = *resp.getHeaders();
                        self->downstream_->sendHeaders(respHeaders);
                        self->downstream_->sendEOM();
                        return;
                    }
                    else
                    {
                        XLOGF(DBG0, "Content length {} bytes for readObject GET of {}", rangeEnd, self->keyInfo.key);
                        resp.send();
                    }
                                              });

                if (self->request_type == RequestType::HeadObject)
                    return;

                self->extents = std::move(res.extents);
                self->done_bytes = rangeStart;
                self->put_remaining.store(rangeEnd, std::memory_order_relaxed);

                self->readObject(evb, std::move(self), rangeStart);
            });
}

void S3Handler::putObject(proxygen::HTTPMessage& headers)
{
    request_type = RequestType::PutObject;
    auto evb = folly::EventBaseManager::get()->getEventBase();
    folly::getGlobalCPUExecutor()->add(
            [self = this->self, evb]()
            {
                if(!self->evpMdCtx.init(EVP_md5()))
                {
                    evb->runInEventBaseThread([self = self]()
                                              {
                        ResponseBuilder(self->downstream_)
                            .status(500, "Internal error")
                            .body(fmt::format("Could not initialize md5"))
                            .sendWithEOM();
                        std::lock_guard lock(self->extents_mutex);
                        self->finished_ = true;
                        self->extents_cond.notify_all(); });
                    return;
                }

                if(self->withBucketVersioning)
                    self->keyInfo.version = self->sfs.get_next_version();

                if(self->keyInfo.key.size()>1024)
                {
                    evb->runInEventBaseThread([self = self]()
                                              {
                        ResponseBuilder(self->downstream_)
                            .status(500, "Internal error")
                            .body(fmt::format("Key too long"))
                            .sendWithEOM();
                        std::lock_guard lock(self->extents_mutex);
                        self->finished_ = true;
                        self->extents_cond.notify_all(); });
                    return;
                }

                const auto fpath = make_key(self->keyInfo);

                auto res = self->sfs.write_prepare(fpath, self->put_remaining);
                if (res.err != 0)
                {                    
                    evb->runInEventBaseThread([self = self, res]()
                                              {
                        ResponseBuilder(self->downstream_)
                            .status(500, "Internal error")
                            .body(fmt::format("Error preparing writing. Errno {}", res.err))
                            .sendWithEOM();
                        std::lock_guard lock(self->extents_mutex);
                        self->finished_ = true;
                        self->extents_cond.notify_all(); });
                    return;
                }

                std::lock_guard lock(self->extents_mutex);
                
                if(!res.extents.empty())
                    self->extents = std::move(res.extents);

                self->extentsInitialized = true;
                self->extents_cond.notify_all();
            });
}

std::pair<std::string, std::string> splitUploadId(const std::string& uploadId)
{
    auto dashIdx = uploadId.find('-');
    if(dashIdx != std::string::npos)
    {
        return std::make_pair(uploadId.substr(0, dashIdx), 
            uploadId.substr(dashIdx+1));
    }
    else
    {
        return std::make_pair(uploadId, uploadId);
    }
}

void S3Handler::putObjectPart(proxygen::HTTPMessage& headers, int partNumber, int64_t uploadId, std::string uploadVerId)
{
    request_type = RequestType::PutObject;
    auto evb = folly::EventBaseManager::get()->getEventBase();
    folly::getGlobalCPUExecutor()->add(
            [self = this->self, evb, partNumber, uploadId, uploadVerId]()
            {
                if(!self->evpMdCtx.init(EVP_md5()))
                {
                    evb->runInEventBaseThread([self = self]()
                                              {
                        ResponseBuilder(self->downstream_)
                            .status(500, "Internal error")
                            .body(fmt::format("Could not initialize md5"))
                            .sendWithEOM();
                        std::lock_guard lock(self->extents_mutex);
                        self->finished_ = true;
                        self->extents_cond.notify_all(); });
                    return;
                }

                const auto partialUploadsBucketId = buckets::getPartialUploadsBucket(self->keyInfo.bucketId);
                const auto partsBucketId = buckets::getPartsBucket(self->keyInfo.bucketId);

                auto readRes = self->sfs.read_prepare(make_key({.key = uploadIdToStr(uploadId), .version = 0, 
                    .bucketId = partialUploadsBucketId}), 0);
                if(readRes.err!=0)
                {
                    evb->runInEventBaseThread([self = self, readRes]()
                                              {
                        ResponseBuilder(self->downstream_)
                            .status(500, "Internal error")
                            .body(fmt::format("Could not find upload data."))
                            .sendWithEOM();
                        std::lock_guard lock(self->extents_mutex);
                        self->finished_ = true;
                        self->extents_cond.notify_all(); });
                    return;
                }

                CRData rdata(readRes.md5sum.data(), readRes.md5sum.size());

                std::string dataPath, dataUploadVerId;

                if(!rdata.getStr2(&dataPath) || !rdata.getStr2(&dataUploadVerId) ||
                    dataPath != make_key(self->keyInfo) || dataUploadVerId!=uploadVerId)
                {
                    evb->runInEventBaseThread([self = self, readRes]()
                                              {
                        ResponseBuilder(self->downstream_)
                            .status(500, "Internal error")
                            .body(fmt::format("Could not verify upload data."))
                            .sendWithEOM();
                        std::lock_guard lock(self->extents_mutex);
                        self->finished_ = true;
                        self->extents_cond.notify_all(); });
                    return;
                }

                self->keyInfo = {.key = uploadIdToStr(uploadId) +"."+uploadIdToStr(partNumber), .version = 0, 
                    .bucketId = partsBucketId};
                auto res = self->sfs.write_prepare(make_key(self->keyInfo), self->put_remaining);
                if (res.err != 0)
                {                    
                    evb->runInEventBaseThread([self = self, res]()
                                              {
                        ResponseBuilder(self->downstream_)
                            .status(500, "Internal error")
                            .body(fmt::format("Error preparing writing. Errno {}", res.err))
                            .sendWithEOM();
                        std::lock_guard lock(self->extents_mutex);
                        self->finished_ = true;
                        self->extents_cond.notify_all(); });
                    return;
                }

                for(const auto& ext: res.extents)
                {
                    assert(ext.len>0);
                }

                if(!res.extents.empty())
                    XLOGF(INFO, "Write object prepare done partNumber {} size {} ext offset={} len={}", partNumber, self->put_remaining.load(std::memory_order_relaxed), res.extents[0].data_file_offset, res.extents[0].len);

                std::lock_guard lock(self->extents_mutex);
                self->extents = std::move(res.extents);
                self->extentsInitialized = true;
                self->extents_cond.notify_all();
            });
}

void S3Handler::createMultipartUpload(proxygen::HTTPMessage& headers)
{
    auto evb = folly::EventBaseManager::get()->getEventBase();
    folly::getGlobalCPUExecutor()->add(
            [self = this->self, evb]()
            {
                auto [uploadId, uploadIdEnc] = self->sfs.get_next_partid();
                auto uploadVerId = random_uuid_binary();

                CWData data;
                data.addString2(make_key(self->keyInfo));
                data.addString2(uploadVerId);

                std::string md5sum(data.getDataPtr(), data.getDataSize());
                const auto partialUploadsBucketId = buckets::getPartialUploadsBucket(self->keyInfo.bucketId);

                auto rc = self->sfs.write(make_key({.key = uploadIdToStr(uploadId), .version = 0, 
                    .bucketId = partialUploadsBucketId}), nullptr, 0, 0, 0, md5sum,  false, false);
         
                evb->runInEventBaseThread([rc=rc, uploadIdEnc = uploadIdEnc, uploadVerId, self = self]()
                                  {   
                                    if(rc!=0)        
                                    {
                        ResponseBuilder(self->downstream_)
                        .status(500, "Internal error")
                        .body("Write finalization error")
                        .sendWithEOM();
                                    }
                                    else
                                    {
                                        std::string resp = fmt::format("<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
        "<InitiateMultipartUploadResult>"
   "\t<Bucket>{}</Bucket>"
   "\t<Key>{}</Key>"
   "\t<UploadId>{}-{}</UploadId>"
"</InitiateMultipartUploadResult>", buckets::getBucketName(self->keyInfo.bucketId), self->keyInfo.key, folly::hexlify(uploadIdEnc), folly::hexlify(uploadVerId));

                                       ResponseBuilder(self->downstream_)
                        .status(200, "OK")
                        .body(resp)
                        .sendWithEOM(); 
                                    }
                    self->finished_ = true; });
                    return;
            });
}

void S3Handler::finalizeMultipartUpload()
{
    assert(multiPartUploadData.get()!=nullptr);
    auto evb = folly::EventBaseManager::get()->getEventBase();
    folly::getGlobalCPUExecutor()->add(
            [self = this->self, evb, multiPartData = this->multiPartUploadData.get()]()
            {
                const auto partialUploadsBucketId = buckets::getPartialUploadsBucket(self->keyInfo.bucketId);
                auto readRes = self->sfs.read_prepare(
                    make_key({.key = uploadIdToStr(multiPartData->uploadId), .version = 0, 
                    .bucketId = partialUploadsBucketId}), 0);
                CRData uploadData(readRes.md5sum.data(), readRes.md5sum.size());
                std::string uploadFPath;
                std::string uploadVerId;
                if(readRes.err != 0 ||
                    !uploadData.getStr2(&uploadFPath) ||
                    uploadFPath!=make_key(self->keyInfo) ||
                    !uploadData.getStr2(&uploadVerId) ||
                    uploadVerId!=multiPartData->verId )
                {
                    evb->runInEventBaseThread([self = self, readRes = readRes]()
                                              {
                        ResponseBuilder(self->downstream_)
                            .status(500, "Internal error")
                            .body(fmt::format("Could not find upload. Errno {}", readRes.err))
                            .sendWithEOM();
                        self->finished_ = true; });
                    return;
                }

                if(self->withBucketVersioning)
                {
                    self->keyInfo.version = self->sfs.get_next_version();
                    uploadFPath = make_key(self->keyInfo);
                }

                MultiPartDownloadData::PartExt lastExt = {-1, 0, 0};
                CWData wdata;
                wdata.addChar(metadata_multipart_object);
                const size_t md5sumOffset = wdata.getDataSize();
                wdata.resize(wdata.getDataSize()+MD5_DIGEST_LENGTH);
                wdata.addVarInt(multiPartData->uploadId);
                wdata.addVarInt(multiPartData->parts.size());

                EVP_MD_CTX* md5Ctx = EVP_MD_CTX_new();
                SCOPE_EXIT { EVP_MD_CTX_free(md5Ctx); };
                if(md5Ctx==nullptr ||
                    EVP_DigestInit(md5Ctx, EVP_md5())!=1)
                {
                    evb->runInEventBaseThread([self = self]()
                                              {
                        ResponseBuilder(self->downstream_)
                            .status(500, "Internal error")
                            .body(fmt::format("Error initializing hash function"))
                            .sendWithEOM();
                        self->finished_ = true; });
                    return;
                }

                const auto partsBucketId = buckets::getPartsBucket(self->keyInfo.bucketId);

                for(const auto& part: multiPartData->parts)
                {
                    if(part.partNumber==7)
                        int abc=5;

                    auto readPartRes = self->sfs.read_prepare(
                        make_key({.key = uploadIdToStr(multiPartData->uploadId)+"."+uploadIdToStr(part.partNumber), .version = 0, 
                            .bucketId = partsBucketId}), SingleFileStorage::ReadSkipAddReading);

                    if(readPartRes.err!=0)
                    {
                        evb->runInEventBaseThread([self = self, partNumber = part.partNumber]()
                                              {
                        ResponseBuilder(self->downstream_)
                            .status(500, "Internal error")
                            .body(fmt::format("Could not find part {}", partNumber))
                            .sendWithEOM();
                        self->finished_ = true; });
                        return;
                    }

                    //TODO: check etag

                    EVP_DigestUpdate(md5Ctx, readPartRes.md5sum.data(), readPartRes.md5sum.size());

                    if(lastExt.len==0)
                    {
                        lastExt.size = readPartRes.total_len;
                        lastExt.start = part.partNumber;
                        lastExt.len = 1;
                    }
                    else
                    {
                        if(lastExt.start + lastExt.len == part.partNumber &&
                            lastExt.size == readPartRes.total_len)
                        {
                            ++lastExt.len;
                        }
                        else
                        {
                            wdata.addVarInt(lastExt.size);
                            wdata.addVarInt(lastExt.start);
                            wdata.addVarInt(lastExt.len);
                            lastExt.size = readPartRes.total_len;
                            lastExt.start = part.partNumber;
                            lastExt.len = 1;
                        }
                    }
                }

                if(lastExt.len>0)
                {
                    wdata.addVarInt(lastExt.size);
                    wdata.addVarInt(lastExt.start);
                    wdata.addVarInt(lastExt.len);
                }

                EVP_DigestFinal_ex(md5Ctx, reinterpret_cast<unsigned char*>(wdata.getDataPtr()) + md5sumOffset, nullptr);

                std::string etagBin(wdata.getDataPtr() + md5sumOffset, wdata.getDataPtr() + md5sumOffset + MD5_DIGEST_LENGTH);
                std::string md5sum(wdata.getDataPtr(), wdata.getDataSize());

                const auto tnow = std::chrono::system_clock::now();
                const auto lastModified = std::chrono::duration_cast<std::chrono::nanoseconds>(
                   tnow.time_since_epoch()).count();

                auto writeRes = self->sfs.write_finalize(uploadFPath, {SingleFileStorage::Ext(0, 0, 0)}, lastModified, md5sum, false, true);
                if(writeRes)
                {
                     evb->runInEventBaseThread([self = self, writeRes]()
                                              {
                        ResponseBuilder(self->downstream_)
                            .status(500, "Internal error")
                            .body(fmt::format("Error adding object to db. Err {}", writeRes))
                            .sendWithEOM();
                        self->finished_ = true; });
                        return;
                }

                auto delRes = self->sfs.del(
                    make_key({.key = uploadIdToStr(multiPartData->uploadId), .version = 0, 
                        .bucketId = partialUploadsBucketId}), SingleFileStorage::DelAction::Del, false);

                if(delRes)
                {
                    evb->runInEventBaseThread([self = self, delRes]()
                                              {
                        ResponseBuilder(self->downstream_)
                            .status(500, "Internal error")
                            .body(fmt::format("Error deleting upload metadata. Err {}", delRes))
                            .sendWithEOM();
                        self->finished_ = true; });
                        return;
                }

                if(!self->commit())
                {
                    evb->runInEventBaseThread([self = self]()
                                        {           
                            ResponseBuilder(self->downstream_)
                                .status(500, "Internal error")
                                .body("Commit error")
                                .sendWithEOM();
                            self->finished_ = true; });
                    return;
                }

                evb->runInEventBaseThread([self = self, etagBin, numParts = multiPartData->parts.size()]()
                                              {
                                                const auto bucket = buckets::getBucketName(self->keyInfo.bucketId);
                        ResponseBuilder(self->downstream_)
                            .status(200, "OK")
                            .body(fmt::format("<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
            "<CompleteMultipartUploadResult xmlns=\"http://s3.amazonaws.com/doc/2006-03-01/\">\n"
             "\t<Location>{}/{}/{}</Location>\n"
             "\t<Bucket>{}</Bucket>\n"
             "\t<Key>{}</Key>\n"
             "\t<ETag>\"{}-{}\"</ETag>\n"
            "</CompleteMultipartUploadResult>", self->serverUrl, bucket, self->keyInfo.key, bucket, self->keyInfo.key, folly::hexlify(etagBin), numParts))
                            .sendWithEOM();
                        self->finished_ = true; });
                        return;
            });    
}

bool S3Handler::commit()
{
    if(self->sfs.get_manual_commit() && FLAGS_commit_after_ms>0)
    {
        static std::mutex mutex;
        static std::set<SingleFileStorage*> commitAfterStarted;

        bool doStart = false;

        {
            std::lock_guard lock(mutex);
            const auto [it, inserted] = commitAfterStarted.insert(&self->sfs);
            if(inserted)
                doStart = true;
        }

        if(doStart)
        {
            folly::getGlobalCPUExecutor()->add([sfs = &self->sfs]() {

                std::this_thread::sleep_for(std::chrono::milliseconds(FLAGS_commit_after_ms));

                {
                    std::lock_guard lock(mutex);
                    commitAfterStarted.erase(sfs);
                }

                const auto startime = std::chrono::steady_clock::now();
                const auto rc = sfs->commit(false, -1, FLAGS_pre_sync_commit);
                const auto duration = std::chrono::steady_clock::now() - startime;

                if(!rc)
                {
                    XLOGF(ERR, "Commit after {} ms failed. Commit duration {}ms", FLAGS_commit_after_ms, std::chrono::duration_cast<std::chrono::milliseconds>(duration).count());
                    abort();
                }
                else
                {
                    XLOGF(INFO, "Commit after {} ms succeeded. Commit duration {}ms", FLAGS_commit_after_ms, std::chrono::duration_cast<std::chrono::milliseconds>(duration).count());
                }                    
            });
        }
    }
    else if(!self->sfs.get_manual_commit())
    {
        const bool b = self->sfs.commit(false, -1, FLAGS_pre_sync_commit);

        if(!b)
        {
            XLOGF(ERR, "Storage commit failed");
            return false;
        }
    }

    return true;
}


void S3Handler::finalizeCreateBucket()
{
    auto evb = folly::EventBaseManager::get()->getEventBase();
    folly::getGlobalCPUExecutor()->add(
            [self = this->self, evb]()
            {
                const auto bucketId = buckets::addBucket(self->keyInfo.key, true);

                if(bucketId < 0)
                {
                    evb->runInEventBaseThread([self = self]()
                                              {
                        ResponseBuilder(self->downstream_)
                            .status(409, "OK")
                            .body("BucketAlreadyExists")
                            .sendWithEOM();
                        self->finished_ = true;
                    });
                    return;
                }                

                evb->runInEventBaseThread([self = self]()
                                              {
                        ResponseBuilder(self->downstream_)
                            .status(200, "OK")
                            .header(HTTPHeaderCode::HTTP_HEADER_LOCATION, fmt::format("/{}", self->keyInfo.key))
                            .sendWithEOM();
                        self->finished_ = true;
                });
                return;
            }
        );
}

void S3Handler::deleteObjects()
{
    auto evb = folly::EventBaseManager::get()->getEventBase();
    folly::getGlobalCPUExecutor()->add(
            [self = this->self, evb, deleteObjectsData = this->deleteObjectsData.get()]()
            {
                const auto bucketName = buckets::getBucketName(self->keyInfo.bucketId);

                std::string resp = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
                                    "<DeleteResult>\n";

                for(const auto& obj: deleteObjectsData->objects)
                {
                    auto version = obj.versionId.value_or(0);
                    const auto resource = fmt::format("arn:aws:s3:::{}/{}", bucketName, obj.key);
                    const auto action = "s3:DeleteObject";

                    if(!isAuthorized(resource, action, deleteObjectsData->accessKey))
                    {
                        resp += fmt::format("\t<Error>\n"
                            "\t\t<Code>AccessDenied</Code>\n"
                            "\t\t<Message>Access to object not authorized</Message>\n"
                            "\t\t<Key>{}</Key>\n"
                            "\t\t</Error>\n", obj.key);
                        continue;
                    }

                    bool delNewest = false;

                    if(self->withBucketVersioning && version==0)
                    {
                        version= std::numeric_limits<int64_t>::max();
                        delNewest = true;
                    }

                    const auto currPath = make_key(obj.key, self->keyInfo.bucketId, version);

                    int res;

                    if(delNewest)
                    {
                        res = self->sfs.check_existence(currPath, SingleFileStorage::ReadNewest);

                        if(res==0)
                        {
                            version = self->sfs.get_next_version();
                            const auto writePath = make_key(obj.key, self->keyInfo.bucketId, version);
                            std::string md5sum(1, metadata_tombstone);
                            res = self->sfs.write(writePath, nullptr, 0, 0, 0, md5sum, false, false);
                        }
                    }
                    else
                    {
                        res = self->sfs.del(currPath, SingleFileStorage::DelAction::Del, false);
                    }

                    if(res==0)
                    {
                        res = self->commit() ? 0 : 1;
                    }

                    if(res==0)
                    {
                        resp += fmt::format("\t<Deleted>\n"
                            "\t\t<Key>{}</Key>\n"
                            "\t</Deleted>\n", obj.key);
                    }
                    else if(res==ENOENT)
                    {
                        resp += fmt::format("\t<Error>\n"
                            "\t\t<Code>NoSuchKey</Code>\n"
                            "\t\t<Message>Object not found</Message>\n"
                            "\t\t<Key>{}</Key>\n"
                            "\t</Error>\n", obj.key);
                    }
                    else
                    {
                        resp += fmt::format("\t<Error>\n"
                            "\t\t<Code>InternalError</Code>\n"
                            "\t\t<Message>Internal error {}</Message>\n"
                            "\t\t<Key>{}</Key>\n"
                            "\t</Error>\n", res, obj.key);
                    }
                }

                resp += "</DeleteResult>";

                 evb->runInEventBaseThread([self = self, resp = std::move(resp)] ()
                 {
                    ResponseBuilder(self->downstream_)
                            .status(200, "OK")
                            .body(resp)
                            .sendWithEOM();
                    self->finished_ = true;
                 });
            }
        );
}

void S3Handler::deleteObject(proxygen::HTTPMessage& headers)
{
    auto evb = folly::EventBaseManager::get()->getEventBase();

    folly::getGlobalCPUExecutor()->add(
        [self = this->self, evb]()
        {
            XLOGF(INFO, "Removing object {}", self->keyInfo.key);

            bool delNewest = false;
            

            if(self->withBucketVersioning && self->keyInfo.version==0)
            {
                self->keyInfo.version = std::numeric_limits<int64_t>::max();
                delNewest = true;
            }

            const auto currPath = make_key(self->keyInfo);
            int res;

            if(delNewest)
            {
                res = self->sfs.check_existence(currPath, SingleFileStorage::ReadNewest);

                if(res==0)
                {
                    self->keyInfo.version = self->sfs.get_next_version();
                    const auto writePath = make_key(self->keyInfo);
                    std::string md5sum(1, metadata_tombstone);
                    res = self->sfs.write(writePath, nullptr, 0, 0, 0, md5sum, false, false);
                }
            }
            else
            {
                res = self->sfs.del(currPath, SingleFileStorage::DelAction::Del, false);
            }

            if(res==0)
            {
                res = self->commit() ? 0 : 1;
            }

            evb->runInEventBaseThread([self = self, res]()
                                            {
                    if(res==ENOENT)
                    {
                        XLOGF(INFO, "Removing object '{}' not found", self->keyInfo.key);
                        ResponseBuilder(self->downstream_)
                            .status(404, "Not found")
                            .body(fmt::format("Object not found"))
                            .sendWithEOM();
                    }
                    else if(res!=0)
                    {
                        XLOGF(INFO, "Removing object '{}' err {}", self->keyInfo.key, res);
                        ResponseBuilder(self->downstream_)
                            .status(500, "Internal error")
                            .body(fmt::format("Storage is dead"))
                            .sendWithEOM();
                    }
                    else
                    {
                        ResponseBuilder(self->downstream_)
                            .status(200, "OK")
                            .sendWithEOM();
                    }
                    self->finished_ = true;
                                            });

        });
}

void S3Handler::deleteBucket(proxygen::HTTPMessage& headers)
{
    auto evb = folly::EventBaseManager::get()->getEventBase();

    folly::getGlobalCPUExecutor()->add(
        [self = this->self, evb]()
        {
            XLOGF(INFO, "Deleting bucket {}", self->keyInfo.key);

            const auto bucketId = buckets::getBucket(self->keyInfo.key);
            bool hasObjects = true;
            bool bucketNotFound = false;
            if(bucketId)
            {
                const auto iterStartVal = make_key({.key = {}, .version=std::numeric_limits<int64_t>::max(), .bucketId = *bucketId});
                SingleFileStorage::IterData iter_data = {};

                self->sfs.list_commit();
                
                if(!self->sfs.iter_start(iterStartVal, false, iter_data))
                {
                    evb->runInEventBaseThread([self = self]()
                                                        {
                                    ResponseBuilder(self->downstream_)
                                        .status(500, "Internal error")
                                        .body(fmt::format("Error starting listing"))
                                        .sendWithEOM(); });
                    return;
                }

                std::string keyBin, md5sum;
                int64_t offset, size, last_modified;
                std::vector<SingleFileStorage::SPunchItem> extra_exts;
                if(!self->sfs.iter_curr_val(keyBin, offset, size, extra_exts, last_modified, md5sum, iter_data))
                {
                    hasObjects = false;
                }
                else
                {
                    const auto keyInfo = extractKeyInfoView(keyBin);
                    hasObjects = keyInfo.bucketId == *bucketId;
                }

                self->sfs.iter_stop(iter_data);

                if(!hasObjects)
                {
                    // TODO: Make sure there is no object currently being added and prevent adding new ones
                    if(!buckets::deleteBucket(*bucketId))
                        bucketNotFound = true;
                }
            }

            evb->runInEventBaseThread([self = self, bucketId, hasObjects, bucketNotFound]()
                                            {
                    if(!bucketId || bucketNotFound)
                    {
                        XLOGF(INFO, "Bucket '{}' not found", self->keyInfo.key);
                        ResponseBuilder(self->downstream_)
                            .status(404, "Not found")
                            .body(fmt::format("Bucket not found"))
                            .sendWithEOM();
                    }
                    else if(hasObjects)
                    {
                        XLOGF(INFO, "Bucket '{}' contains objects", self->keyInfo.key);
                        ResponseBuilder(self->downstream_)
                            .status(400, "BucketContainsObjects")
                            .body(fmt::format("Bucket contains objects"))
                            .sendWithEOM();
                    }
                    else
                    {
                        ResponseBuilder(self->downstream_)
                            .status(200, "OK")
                            .sendWithEOM();
                    }
                                            }
                                        );
        }
    );
}

int S3Handler::seekMultipartExt(SingleFileStorage& sfs, int64_t offset, int64_t bucketId, MultiPartDownloadData& multiPartDownloadData, std::vector<SingleFileStorage::Ext>& extents)
{
    if(multiPartDownloadData.exts.empty())
    {
        XLOG(WARN, "No multi-part parts found in seek");
        return ENOENT;
    }

    XLOGF(DBG0, "Seeking multi-part to offset {}", offset);

    multiPartDownloadData.extIdx = 0;
    multiPartDownloadData.currExt = multiPartDownloadData.exts[multiPartDownloadData.extIdx];

    int64_t seekOffset = 0;

    while(seekOffset<offset)
    {        
        int64_t toSeek = offset - seekOffset;
        int64_t numSeek = toSeek/multiPartDownloadData.currExt.size;
        
        if(numSeek>multiPartDownloadData.currExt.len)
        {
            seekOffset+=multiPartDownloadData.currExt.size*multiPartDownloadData.currExt.len;
            XLOGF(DBG0, "Seeking forward to next multi-part ext seekOffset {}", seekOffset);            
            ++multiPartDownloadData.extIdx;
            if(multiPartDownloadData.extIdx>=multiPartDownloadData.exts.size())
            {
                XLOGF(WARN, "Out of multi-part parts size {} in seek", multiPartDownloadData.exts.size());
                return ENOENT;
            }
            multiPartDownloadData.currExt = multiPartDownloadData.exts[multiPartDownloadData.extIdx];
        }
        else
        {
            XLOGF(DBG0, "Seeking forward {} exts in current multi-part ext", numSeek);
            multiPartDownloadData.currExt.start += numSeek;
            multiPartDownloadData.currExt.len -= numSeek;
            seekOffset+=numSeek*multiPartDownloadData.currExt.size;
            break;
        }
    }

    return readMultipartExt(sfs, seekOffset, bucketId, multiPartDownloadData, extents);
}

int S3Handler::readMultipartExt(SingleFileStorage& sfs, int64_t offset, int64_t bucketId, MultiPartDownloadData& multiPartDownloadData, std::vector<SingleFileStorage::Ext>& extents)
{
    auto partNum = multiPartDownloadData.currExt.start;
    const auto partsBucketId = buckets::getPartsBucket(bucketId);
    auto res = sfs.read_prepare(
        make_key({.key = uploadIdToStr(multiPartDownloadData.uploadId)+"."+uploadIdToStr(partNum), .version = 0, 
                    .bucketId = partsBucketId}), 0);
    if (res.err != 0)
    {
        XLOGF(WARN, "Error reading next multipart object meta-information uploadid {} partnum {}", multiPartDownloadData.uploadId, partNum);
        return EIO;
    }

    multiPartDownloadData.needsFinalize = true;

    extents = std::move(res.extents);

    multiPartDownloadData.currOffset = offset;

    int64_t size = 0;

    for(auto& ext: extents)
    {
        ext.obj_offset+=offset;
        size += ext.len;
    }

    XLOGF(DBG0, "Multi-part ext offset {} partNum {} extents {} size {}", offset, partNum, extents.size(), size);

    return 0;
}

int S3Handler::readNextMultipartExt(SingleFileStorage& sfs, int64_t offset, int64_t bucketId, MultiPartDownloadData& multiPartDownloadData, std::vector<SingleFileStorage::Ext>& extents)
{
    int lastPartNum = -1;
    if(multiPartDownloadData.extIdx == std::string::npos)
    {
        if(multiPartDownloadData.exts.empty())
        {
            XLOG(WARN, "No multi-part parts found");
            return ENOENT;
        }

        multiPartDownloadData.extIdx = 0;
        multiPartDownloadData.currExt = multiPartDownloadData.exts[multiPartDownloadData.extIdx];
    }
    else
    {
        lastPartNum = multiPartDownloadData.currExt.start;
        ++multiPartDownloadData.currExt.start;
        --multiPartDownloadData.currExt.len;
    }

    while(multiPartDownloadData.currExt.len<=0)
    {
        if(multiPartDownloadData.extIdx + 1 >= multiPartDownloadData.exts.size())
        {
            XLOGF(WARN, "Out of multi-part parts size {} while seeking to offset {}", multiPartDownloadData.exts.size(), offset);
            return ENOENT;
        }
        
        ++multiPartDownloadData.extIdx;
        multiPartDownloadData.currExt = multiPartDownloadData.exts[multiPartDownloadData.extIdx];
    }

    
    if(!extents.empty() && extents[0].len>0 && multiPartDownloadData.needsFinalize)
    {
        for(auto& ext: extents)
        {
            assert(ext.obj_offset>=multiPartDownloadData.currOffset );
            ext.obj_offset-=multiPartDownloadData.currOffset;
        }
        const auto partsBucketId = buckets::getPartsBucket(bucketId);
        const auto rc = sfs.read_finalize(
            make_key({.key = uploadIdToStr(multiPartDownloadData.uploadId)+"."+uploadIdToStr(lastPartNum), .version = 0, 
                    .bucketId = partsBucketId}), extents, 0);
        assert(rc==0);
        multiPartDownloadData.needsFinalize = false;
    }

    const auto rc = readMultipartExt(sfs, offset, bucketId, multiPartDownloadData, extents);
    if(rc)
    {
        XLOGF(WARN, "Error reading next multipart extents code {}", rc);
        return rc;
    }

    return 0;
}

int S3Handler::finalizeMultiPart(SingleFileStorage& sfs, const int64_t bucketId, MultiPartDownloadData& multiPartDownloadData, std::vector<SingleFileStorage::Ext>& extents)
{
    if(multiPartDownloadData.extIdx == std::string::npos)
        return ENOENT;

    if(!multiPartDownloadData.needsFinalize)
        return 0;

    const auto partsBucketId = buckets::getPartsBucket(bucketId);
    const auto partNum = multiPartDownloadData.currExt.start;

    if(!extents.empty() && extents[0].len>0)
    {
        for(auto& ext: extents)
        {
            assert(ext.obj_offset>=multiPartDownloadData.currOffset );
            ext.obj_offset-=multiPartDownloadData.currOffset;
        }
        
        const auto rc = sfs.read_finalize(make_key({.key = uploadIdToStr(multiPartDownloadData.uploadId)+"."+uploadIdToStr(partNum), .version = 0, 
                    .bucketId = partsBucketId}), extents, 0);
        assert(rc==0);
    }

    multiPartDownloadData.needsFinalize = false;

    return 0;
}

void S3Handler::readObject(folly::EventBase *evb, std::shared_ptr<S3Handler> self, int64_t offset)
{
    const size_t bufsize = 32768;
    folly::IOBufQueue buf;

    if(multiPartDownloadData)
    {
        const auto rc = seekMultipartExt(self->sfs, offset, self->keyInfo.bucketId, *self->multiPartDownloadData, self->extents);
        if(rc)
        {
            XLOGF(WARN, "Error seeking to part rc {}", rc);
            evb->runInEventBaseThread([self = self]() mutable
                                {
                self->downstream_->sendAbort();
                self->finished_ = true;
                self->running = false;
                                    } );
            return;
        }
    }

    bool did_pause = false;
    bool has_error = false;
    while(offset < put_remaining.load(std::memory_order_relaxed))
    {
        if (self->paused_)
        {
            XLOGF(DBG0, "Sending of {} paused at {} done bytes. Finished={} Running={}", self->keyInfo.key, self->done_bytes, self->finished_, self->running);
            did_pause = true;
            break;
        }

        auto it = std::upper_bound(extents.begin(), extents.end(), SingleFileStorage::Ext(offset, 0, 0));
        if(!extents.empty())
            --it;
        assert(it != extents.end());
        if(it==extents.end() || it->len==0 || it->obj_offset + it->len <= offset)
        {
            if(multiPartDownloadData)
            {
                const int rc = readNextMultipartExt(self->sfs, offset, self->keyInfo.bucketId, *self->multiPartDownloadData, self->extents);
                if(rc)
                {
                    XLOGF(WARN, "Error reading next part code {} while reading object {}", rc, self->keyInfo.key);
                    evb->runInEventBaseThread([self = self]() mutable
                                      {
                        self->downstream_->sendAbort();
                        self->finished_ = true;
                        self->running = false;
                                            } );
                    has_error = true;
                    break;
                }
                continue;
            }
            break;
        }

        assert(it->obj_offset <= offset && it->obj_offset + it->len > offset);
        
        int64_t ext_offset = offset - it->obj_offset;
        auto curr_ext = SingleFileStorage::Ext(it->obj_offset + ext_offset, it->data_file_offset + ext_offset, it->len - ext_offset);
        int64_t rlen = std::min(static_cast<int64_t>(bufsize), put_remaining.load(std::memory_order_relaxed) - offset);

        auto res = sfs.read_ext(curr_ext, 0, static_cast<size_t>(rlen), buf);

        if(res.err!=0)
        {
            XLOGF(WARN, "Error reading extent code {}", res.err);
            evb->runInEventBaseThread([self = self]() mutable
                                      {
                self->downstream_->sendAbort();
                self->finished_ = true;
                self->running = false;
                                      } );
            has_error = true;
            break;
        }
    
        if(multiPartDownloadData)
        {
            XLOGF(DBG0, "Sending body off {} len {} of fpath {} total_len {} part {}", offset, res.buf->length(), self->keyInfo.key, put_remaining.load(std::memory_order_relaxed), multiPartDownloadData->currExt.start);
        }
        else
        {
            XLOGF(DBG0, "Sending body off {} len {} of fpath {} total_len {}", offset, res.buf->length(), self->keyInfo.key, put_remaining.load(std::memory_order_relaxed));
        }

        offset += res.buf->length();

        evb->runInEventBaseThread([self = self, body = std::move(res.buf), total_len = put_remaining.load(std::memory_order_relaxed)]() mutable
                                      {
            if(self->finished_)
                return;

            self->done_bytes += body->length();
            XLOGF(DBG0, "Done bytes {}", self->done_bytes);
            auto resp = std::move(ResponseBuilder(self->downstream_).body(std::move(body)));
            if(self->done_bytes == total_len)
            {
                resp.sendWithEOM();
                self->finished_ = true;
                self->running = false;
            }
            else
            {
                resp.send();
            }
                                      });


    }

    if(!has_error && offset < put_remaining.load(std::memory_order_relaxed))
    {
        evb->runInEventBaseThread([self = self, did_pause]
                                {
            if(self->finished_)
            {
                // TODO: Add some checks if the ref counting is correct
                int rc;
                if(self->multiPartDownloadData)
                    rc = finalizeMultiPart(self->sfs, self->keyInfo.bucketId, *self->multiPartDownloadData, self->extents);
                else
                    rc = self->sfs.read_finalize(make_key(self->keyInfo), self->extents, 0);
                assert(rc==0);
                return;
            }

            XLOG(DBG0) << "Setting running=false";
            self->running = false;

            if (did_pause)
            {
                XLOG(DBG0) << "Resuming deferred readObject";
                if(!self->paused_ && !self->running)
                {
                    XLOG(DBG0) << "Was unpaused. Resuming.";
                    self->onEgressResumed();
                }
            }

                                });
    }
    else
    {
        int rc;
        if(multiPartDownloadData)
            rc = finalizeMultiPart(self->sfs, self->keyInfo.bucketId, *self->multiPartDownloadData, self->extents);
        else
            rc = sfs.read_finalize(make_key(self->keyInfo), self->extents, 0);
        assert(rc==0);
    }
}

void S3Handler::listObjects(folly::EventBase *evb, std::shared_ptr<S3Handler> self, const std::string& marker,
    const int maxKeys, const std::optional<std::string>& prefix, const std::optional<std::string>& startAfter,
    const std::string& delimiter, const int64_t bucketId,
    const bool listV2, const std::string& bucketName)
{
    SingleFileStorage::IterData iter_data = {};
    std::string iterStartVal;
    if(!marker.empty())
    {
        if(startAfter && *startAfter>marker)
            iterStartVal = make_key({.key = *startAfter, .version=std::numeric_limits<int64_t>::max(), .bucketId = bucketId});
        else
            iterStartVal = make_key({.key = marker, .version=std::numeric_limits<int64_t>::max(), .bucketId = bucketId});
    }
    else
    {
        if(startAfter)
            iterStartVal = make_key({.key = *startAfter, .version=std::numeric_limits<int64_t>::max(), .bucketId = bucketId});
        else if(prefix)
            iterStartVal = make_key({.key = *prefix, .version=std::numeric_limits<int64_t>::max(), .bucketId = bucketId});
        else
            iterStartVal = make_key({.key = {}, .version=std::numeric_limits<int64_t>::max(), .bucketId = bucketId});
    }

    sfs.list_commit();

    if(!sfs.iter_start(iterStartVal, false, iter_data))
    {
        evb->runInEventBaseThread([self = self]()
                                              {
                        ResponseBuilder(self->downstream_)
                            .status(500, "Internal error")
                            .body(fmt::format("Error starting listing"))
                            .sendWithEOM(); });
        return;
    }

    std::string val_data;
    std::vector<std::string> commonPrefixes;
    const size_t prefixSize = prefix ? prefix->size() : 0;

    int i;
    bool truncated = true;
    size_t keyCount = 0;
    std::string lastOutputKeyStr;
    int skippedKeys = 0;
    for(i=0;i<maxKeys + skippedKeys;++i)
    {
        std::string keyBin, md5sum;
        int64_t offset, size, last_modified;
        std::vector<SingleFileStorage::SPunchItem> extra_exts;
        if(!sfs.iter_curr_val(keyBin, offset, size, extra_exts, last_modified, md5sum, iter_data))
        {
            truncated = false;
            break;
        }

        const auto keyInfo = extractKeyInfoView(keyBin);

        if(prefix && !keyInfo.key.starts_with(*prefix))
        {
            truncated = false;
            break;
        }

        if(keyInfo.bucketId != bucketId)
        {
            truncated = false;
            break;
        }

        bool outputKey = true;

        if(keyInfo.key == lastOutputKeyStr)
        {
            ++skippedKeys;
            outputKey = false;
        }
        else if(md5sum.size() == 1 && md5sum[0] == metadata_tombstone)
        {
            lastOutputKeyStr = keyInfo.key;
            ++skippedKeys;
            outputKey = false;
        }

        if(outputKey && !delimiter.empty())
        {
            const size_t delimPos = keyInfo.key.find_first_of(delimiter[0], prefixSize);
            if(delimPos != std::string::npos)
            {
                const auto commonKey = std::string(keyInfo.key.substr(prefixSize, delimPos - prefixSize + 1));
                if(commonKey!=lastOutputKeyStr)                
                    commonPrefixes.push_back(commonKey);
                outputKey = false;
                lastOutputKeyStr = commonKey;
            }
        }

        if (outputKey)
        {
            lastOutputKeyStr = keyInfo.key;

            for(const auto& ext: extra_exts)
            {
                size += ext.len;
            }

            val_data += fmt::format("\t<Contents>\n"
                "\t\t<Key>{}</Key>\n"
                "\t\t<LastModified>{}</LastModified>\n"
                "\t\t<ETag>{}</ETag>\n"
                "\t\t<Size>{}</Size>\n"
                "\t\t<StorageClass>STANDARD</StorageClass>\n"
                "\t\t<Owner>\n"
                "\t\t\t<ID>75aa57f09aa0c8caeab4f8c24e99d10f8e7faeebf76c078efc7c6caea54ba06a</ID>\n"
                "\t\t\t<DisplayName>mtd@amazon.com</DisplayName>\n"
                "\t\t</Owner>\n"
                "\t</Contents>", escapeXML(keyInfo.key), format_last_modified(last_modified), getEtag(md5sum), size);
            ++keyCount;
        }

        if(!sfs.iter_next(iter_data))
        {
            evb->runInEventBaseThread([self = self]()
                                              {
                        ResponseBuilder(self->downstream_)
                            .status(500, "Internal error")
                            .body(fmt::format("Error listing (in iteration)"))
                            .sendWithEOM(); });
            sfs.iter_stop(iter_data);
            return;
        }
    }

    std::string nextMarker;
    if(truncated)
    {
        std::string data;
        std::string keyBin;
        sfs.iter_curr_val(keyBin, data, iter_data);

        const auto keyInfo = extractKeyInfoView(keyBin);
        nextMarker = keyInfo.key;
    }

    sfs.iter_stop(iter_data);

    std::string resp = fmt::format("<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
        "<ListBucketResult>\n"
        "\t<Name>{}</Name>\n"
        "\t<IsTruncated>{}</IsTruncated>\n"
        "\t<MaxKeys>{}</MaxKeys>\n", escapeXML(bucketName), truncated ? "true" : "false", maxKeys);


    if(!listV2)
    {
        resp+=fmt::format("\t<Marker>{}</Marker>\n", escapeXML(marker));
    }
    else if(!marker.empty())
    {
        resp+=fmt::format("\t<ContinuationToken>{}</ContinuationToken>\n", folly::hexlify(marker));
    }

    if(listV2 && truncated)
    {
        resp+=fmt::format("\t<NextContinuationToken>{}</NextContinuationToken>\n", folly::hexlify(nextMarker));
    }
    else if(!listV2 && truncated && !delimiter.empty())
    {
        resp+=fmt::format("\t<NextMarker>{}</NextMarker>\n", escapeXML(nextMarker));
    }

    resp+=val_data;

    if(prefix)
    {
        resp+=fmt::format("\t<Prefix>{}</Prefix>\n", escapeXML(*prefix));
    }

    if(!delimiter.empty())
    {
        resp+=fmt::format("\t<Delimiter>{}</Delimiter>\n", escapeXML(delimiter));
    }

    if(listV2)
    {
        resp+=fmt::format("\t<KeyCount>{}</KeyCount>\n", keyCount);
    }

    if(startAfter)
    {
        resp+=fmt::format("\t<StartAfter>{}</StartAfter>\n", escapeXML(*startAfter));
    }

    if(!commonPrefixes.empty())
    {
        for(const auto commonPrefix: commonPrefixes)
        {
            resp+=fmt::format("\t<CommonPrefixes>\n"
                "\t\t<Prefix>{}</Prefix>\n"
                "\t</CommonPrefixes>\n", escapeXML(commonPrefix));
        }
    }

    resp+="</ListBucketResult>";

    evb->runInEventBaseThread([self = self, resp = std::move(resp)]()
                                              {
                        ResponseBuilder(self->downstream_)
                            .status(200, "OK")
                            .body(resp)
                            .sendWithEOM(); });
}

void S3Handler::onEgressPaused() noexcept
{
    // This will terminate readFile soon
    XLOG(DBG0) << "S3Handler paused";
    paused_ = true;
}

void S3Handler::onEgressResumed() noexcept
{
    XLOG(DBG0) << "S3Handler resumed";
    paused_ = false;
    // If readFileScheduled_, it will reschedule itself
    if (!running && keyInfo.bucketId!=0 && !finished_)
    {
        running = true;
        XLOGF(DBG0, "Starting readObject of {} offset {}", keyInfo.key, done_bytes);
        folly::getGlobalCPUExecutor()->add(
            [self = self, evb = folly::EventBaseManager::get()->getEventBase(), offset = done_bytes]()
            {
                self->readObject(evb, std::move(self), offset);
            });
    }
    else
    {
        XLOGF(DBG0, "Deferred scheduling readFile finished={}", finished_);
    }
}

void S3Handler::readBodyThread(folly::EventBase *evb)
{
    if(finished_)
        return;

    std::unique_lock lock{bodyMutex};
    bool unpause = false;
    size_t cnt = 0;
    while(!bodyQueue.empty())
    {
        if(cnt>2)
        {
            lock.unlock();

            if(unpause)
            {
                evb->runInEventBaseThread([self = this->self]()
                {
                    if(!self)
                        return;

                    self->downstream_->resumeIngress();
                });
            }
            folly::getGlobalCPUExecutor()->add(
                [self = this->self, evb]() mutable
                {
                    if(!self)
                        return;

                    self->readBodyThread(evb);
                });
            return;
        }

        BodyObj obj = std::move(bodyQueue.front());
        bodyQueue.pop();
        lock.unlock();
        onBodyCPU(evb, obj.offset, std::move(obj.body));
        if(obj.unpause)
            unpause = true;
        lock.lock();

        if(bodyQueue.size()<4 && unpause)
        {
            evb->runInEventBaseThread([self = this->self]()
            {
                if(!self)
                    return;

                self->downstream_->resumeIngress();
            });
            unpause=false;
        }
    }
    hasBodyThread = false;

    lock.unlock();

    if(unpause)
    {
        evb->runInEventBaseThread([self = this->self]()
        {
            if(!self)
                return;

            self->downstream_->resumeIngress();
        });
    }
}

void S3Handler::onBody(std::unique_ptr<folly::IOBuf> body) noexcept
{
    if(request_type == RequestType::Unknown)
        return;

    auto evb = folly::EventBaseManager::get()->getEventBase();

    size_t body_bytes = body->length();

    if(request_type == RequestType::CompleteMultipartUpload ||
        request_type == RequestType::DeleteObjects)
    {
        if(payloadHash)
            EVP_DigestUpdate(payloadHash->evpMdCtx.ctx, body->data(), body->length());

        done_bytes += body_bytes;
        const auto isFinal = put_remaining.fetch_sub(body->length(), std::memory_order_release) == body->length();

        if(isFinal && payloadHash && !payloadHash->isFinalExpected())
        {
            ResponseBuilder(self->downstream_)
                .status(500, "Internal error")
                .body("Payload hash not as expected")
                .sendWithEOM();
            finished_ = true; 
            return;
        }

        auto rc = XML_Parse(xmlBody.parser, reinterpret_cast<const char*>(body->data()), body->length(), isFinal ? 1 : 0);
        if(rc!=XML_STATUS_OK)
        {                 
            ResponseBuilder(self->downstream_)
                .status(500, "Internal error")
                .body("Write ext error")
                .sendWithEOM();
            finished_ = true; 
            return;
        }
        return;
    }
    else if(request_type == RequestType::CreateBucket)
    {
        if(payloadHash)
            EVP_DigestUpdate(payloadHash->evpMdCtx.ctx, body->data(), body->length());

        done_bytes += body_bytes;
        const auto isFinal = put_remaining.fetch_sub(body->length(), std::memory_order_release) == body->length();

        if(isFinal && payloadHash && !payloadHash->isFinalExpected())
        {
            ResponseBuilder(self->downstream_)
                .status(500, "Internal error")
                .body("Payload hash not as expected")
                .sendWithEOM();
            finished_ = true; 
            return;
        }
        
        if(bodyData.size() < 10*1024)
        {
            bodyData.append(reinterpret_cast<const char*>(body->data()), body->length());
        }
        else
        {
            ResponseBuilder(self->downstream_)
                .status(500, "Internal error")
                .body("Body too long")
                .sendWithEOM();
            finished_ = true; 
        }
        return;
    }
    else if(request_type != RequestType::PutObject)
    {
        XLOGF(WARN, "Ignoring body received in request type {}", static_cast<int>(request_type));
        return;
    }

    {
        std::scoped_lock lock{bodyMutex};
        const bool pause = bodyQueue.size()>7;
        if(pause)
            downstream_->pauseIngress();
        bodyQueue.emplace(BodyObj{.offset = done_bytes, .body = std::move(body), .unpause = pause});

        if(!hasBodyThread)
        {
            hasBodyThread = true;
            folly::getGlobalCPUExecutor()->add(
                [self = this->self, evb]() mutable
                {
                    self->readBodyThread(evb);
                });
        }
    }

    done_bytes += body_bytes;
}

void S3Handler::onBodyCPU(folly::EventBase *evb, int64_t offset, std::unique_ptr<folly::IOBuf> body)
{
    {
        std::unique_lock lock(extents_mutex);
        while (!extentsInitialized && !finished_)
        {
            extents_cond.wait(lock);
        }

        if (finished_)
        {
            return;
        }
    }

    if(!body)
    {
        if(put_remaining!=0)
        {
            evb->runInEventBaseThread([self = this]()
                                {           
                    if(!self || self->finished_)
                        return;
                    ResponseBuilder(self->downstream_)
                        .status(500, "Internal error")
                        .body("Expecting more data")
                        .sendWithEOM();
                    self->finished_ = true; 
                                });
            return;
        }

        std::string md5sum;
        md5sum.resize(MD5_DIGEST_LENGTH+1);
        md5sum[0] = metadata_object;
        EVP_DigestFinal_ex(evpMdCtx.ctx, reinterpret_cast<unsigned char*>(&md5sum[1]), nullptr);

        if(payloadHash && !payloadHash->isFinalExpected())
        {
            evb->runInEventBaseThread([self = this]()
                                {           
                if(!self || self->finished_)
                    return;
                ResponseBuilder(self->downstream_)
                    .status(400, "Bad request")
                    .body("Invalid hash")
                    .sendWithEOM();
                self->finished_ = true; });
            return;
        }

        if(!extents.empty())
            XLOGF(INFO, "Finalize object {} ext off {} len {}", keyInfo.key, extents[0].data_file_offset, extents[0].len);
        else 
            XLOGF(INFO, "Finalize empty object {}", keyInfo.key);

        const auto tnow = std::chrono::system_clock::now();
        const auto lastModified = std::chrono::duration_cast<std::chrono::nanoseconds>(
                tnow.time_since_epoch()).count();

        auto rc = sfs.write_finalize(make_key(keyInfo), extents, lastModified, md5sum, false, true);

        if (rc != 0)
        {
            evb->runInEventBaseThread([self = this]()
                                {           
                    if(!self || self->finished_)
                        return;
                    ResponseBuilder(self->downstream_)
                        .status(500, "Internal error")
                        .body("Write finalization error")
                        .sendWithEOM();
                    self->finished_ = true; });
            return;
        }

        if(!commit())
        {
            evb->runInEventBaseThread([self = this]()
                                {           
                    if(!self || self->finished_)
                        return;
                    ResponseBuilder(self->downstream_)
                        .status(500, "Internal error")
                        .body("Commit error")
                        .sendWithEOM();
                    self->finished_ = true; });
            return;
        }

        evb->runInEventBaseThread([self = this, md5sum]()
                                {      
                    if(!self)
                        return;
                    ResponseBuilder resp(self->downstream_);
                    resp.status(200, "OK");
                    resp.header(HTTPHeaderCode::HTTP_HEADER_ETAG, fmt::format("\"{}\"", folly::hexlify(md5sum.substr(1))));
                    if(self->keyInfo.version != 0)
                    {
                        resp.header("x-amz-version-id", self->sfs.encrypt_id(self->keyInfo.version));
                        
                    }
                    resp.sendWithEOM();
                    self->finished_ = true; });
        return;
    }

    if(extents.size()>1)
    {
        assert(extents[0] < extents[1]);
    }

    for(const auto& ext: extents)
    {
        assert(ext.len>0);
    }

    const uint8_t *data = body->data();
    size_t data_size = body->length();

    EVP_DigestUpdate(evpMdCtx.ctx, data, data_size);

    if(payloadHash)
    {
        EVP_DigestUpdate(payloadHash->evpMdCtx.ctx, data, data_size);
    }

    while(data_size > 0)
    {
        auto it = std::upper_bound(extents.begin(), extents.end(), SingleFileStorage::Ext(offset, 0, 0));
        if(!extents.empty())
            --it;
        assert(it != extents.end());
        if(it==extents.end())
            break;

        if(!(it->obj_offset <= offset && it->obj_offset + it->len > offset))
        {
            XLOGF(DBG0, "Selected ext obj_offset={} len={} data_file_offset={} offset={} exts={}", it->obj_offset, it->len, it->data_file_offset, offset, extents.size());
            std::sort(extents.begin(), extents.end());
            auto it2= std::upper_bound(extents.begin(), extents.end(), SingleFileStorage::Ext(offset, 0, 0));
            XLOGF(DBG0, "Selected ext obj_offset={} len={} data_file_offset={} offset={} exts={}", it2->obj_offset, it2->len, it2->data_file_offset, offset, extents.size());
            break;
        }
        assert(it->obj_offset <= offset && it->obj_offset + it->len > offset);
        
        int64_t ext_offset = offset - it->obj_offset;
        auto curr_ext = SingleFileStorage::Ext(it->obj_offset + ext_offset, it->data_file_offset + ext_offset, it->len - ext_offset);
        int64_t wlen = std::min(static_cast<int64_t>(data_size), curr_ext.len);

        auto rc = sfs.write_ext(curr_ext, reinterpret_cast<const char*>(data), wlen);
        if (rc != 0)
        {
            evb->runInEventBaseThread([self = this->self]()
                                  {  
                    if(!self || self->finished_)
                        return;     
                    ResponseBuilder(self->downstream_)
                        .status(500, "Internal error")
                        .body("Write ext error")
                        .sendWithEOM();
                    self->finished_ = true; });
            return;
        }

        data += wlen;
        data_size -= wlen;
        offset += wlen;
    }

    assert(data_size == 0);

    put_remaining.fetch_sub(body->length(), std::memory_order_release);
}

void S3Handler::onEOM() noexcept
{
    auto evb = folly::EventBaseManager::get()->getEventBase();

    if(request_type == RequestType::CompleteMultipartUpload)
    {
        if(finished_)
            return;

        if(put_remaining!=0)
        {
            ResponseBuilder(self->downstream_)
                .status(500, "Internal error")
                .body("Expecting more data")
                .sendWithEOM();
            finished_ = true; 
            return;
        }

        finalizeMultipartUpload();
        return;
    }
    else if(request_type == RequestType::PutObject)
    {
        if(finished_)
            return;

        std::scoped_lock lock{bodyMutex};
        bodyQueue.emplace(BodyObj{.offset = done_bytes, .body = {}, .unpause = false});

        if(!hasBodyThread)
        {
            hasBodyThread = true;
            folly::getGlobalCPUExecutor()->add(
                [self = this->self, evb]() mutable
                {
                    self->readBodyThread(evb);
                });
        }
    }
    else if(request_type == RequestType::CreateBucket)
    {
        if(finished_)
            return;

        finalizeCreateBucket();
        return;
    }
    else if(request_type == RequestType::DeleteObjects)
    {
        if(finished_)
            return;

        if(put_remaining!=0)
        {
            ResponseBuilder(self->downstream_)
                .status(500, "Internal error")
                .body("Expecting more data")
                .sendWithEOM();
            finished_ = true; 
            return;
        }

        deleteObjects();
        return;
    }
}

void S3Handler::onUpgrade(UpgradeProtocol /*protocol*/) noexcept
{
    // handler doesn't support upgrades
}

void S3Handler::requestComplete() noexcept
{
    XLOG(DBG0, "Request complete");
    finished_ = true;
    paused_ = true;
    self.reset();
}

void S3Handler::onError(ProxygenError /*err*/) noexcept
{
    XLOG(DBG0, "onError");
    finished_ = true;
    paused_ = true;

    if (request_type == RequestType::PutObject)
    {
        // TODO: Free extents
    }

    self.reset();
}


std::vector<std::string> S3Handler::onDeleteCallback(const std::string& fn, const std::string& md5sum)
{
    const auto keyInfo = extractKeyInfoView(fn);
    if(buckets::isPartialUploadsBucket(keyInfo.bucketId))
    {
        return {};
    }

    int64_t totalLen;
    std::unique_ptr<MultiPartDownloadData> multiPartDownloadData;
    if(!parseMultipartInfo(md5sum, totalLen, multiPartDownloadData))
    {
        XLOGF(WARN, "Error parsing multi-part upload for file {}", fn);
        return {};
    }

    std::vector<std::string> ret;

    if(multiPartDownloadData)
    {
        int parts = 0;
        for(const auto& ext: multiPartDownloadData->exts)
        {
            parts+= ext.len;
        }

        XLOGF(INFO, "Multi-part upload found for object {} with total length {}. Deleting {} parts", keyInfo.key, totalLen, parts);
        const auto partsBucketId = buckets::getPartsBucket(keyInfo.bucketId);
        ret.reserve(parts);

        for(const auto& ext: multiPartDownloadData->exts)
        {
            for(auto partNum=ext.start; partNum<ext.start+ext.len; ++partNum)
            {
                const auto key = make_key({.key = uploadIdToStr(multiPartDownloadData->uploadId)+"."+uploadIdToStr(partNum), .version = 0, 
                            .bucketId = partsBucketId});
                ret.push_back(key);
            }
        }
    }

    return ret;
}
