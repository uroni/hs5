#include <folly/lang/Bits.h>
#include <cstring>
#include "PayloadHash.h"
#include <folly/String.h>
#include <aws/checksums/crc.h>
#include <folly/logging/xlog.h>
#include <openssl/sha.h>

bool PayloadHashBase::isFinalExpected()
{
    const auto gotHash = final();
    const bool ok = expectedHash == gotHash;
    if(ok)
    {
        XLOGF(INFO, "Payload hash {} ok", folly::hexlify(expectedHash));
    }
    else
    {
        XLOGF(INFO, "Payload hash mismatch, expected {}, got {}", folly::hexlify(expectedHash), folly::hexlify(gotHash));
    }
    return ok;
}

// PayloadHashSha256 methods
bool PayloadHashSha256::checkSize()
{
    return expectedHash.size() == SHA256_DIGEST_LENGTH;
}

std::string PayloadHashSha256::final()
{
    std::string binHash;
    binHash.resize(SHA256_DIGEST_LENGTH);
    EVP_DigestFinal_ex(evpMdCtx.ctx, reinterpret_cast<unsigned char*>(&binHash[0]), nullptr);
    return binHash;
}

void PayloadHashSha256::update(const unsigned char* data, const size_t len)
{
    EVP_DigestUpdate(evpMdCtx.ctx, data, len);
}

bool PayloadHashSha256::init()
{
    return evpMdCtx.init(EVP_sha256());
}

// PayloadHashSha1 methods
const size_t SHA1_DIGEST_LENGTH = 20;

bool PayloadHashSha1::checkSize()
{
    return expectedHash.size() == SHA1_DIGEST_LENGTH;
}

std::string PayloadHashSha1::final()
{
    std::string binHash;
    binHash.resize(SHA1_DIGEST_LENGTH);
    EVP_DigestFinal_ex(evpMdCtx.ctx, reinterpret_cast<unsigned char*>(&binHash[0]), nullptr);
    return binHash;
}

void PayloadHashSha1::update(const unsigned char* data, const size_t len)
{
    EVP_DigestUpdate(evpMdCtx.ctx, data, len);
}

bool PayloadHashSha1::init()
{
    return evpMdCtx.init(EVP_sha1());
}

// PayloadHashCrc32 methods
bool PayloadHashCrc32::checkSize()
{
    return expectedHash.size() == sizeof(uint32_t);
}

std::string PayloadHashCrc32::final()
{
    auto val = folly::Endian::big(crc32Value);
    std::string binHash;
    binHash.resize(sizeof(val));    
    memcpy(&binHash[0], &val, sizeof(val));
    return binHash;
}

void PayloadHashCrc32::update(const unsigned char* data, const size_t len)
{
    crc32Value = aws_checksums_crc32(data, len, crc32Value);
}

// PayloadHashCrc32c methods
bool PayloadHashCrc32c::checkSize()
{
    return expectedHash.size() == sizeof(uint32_t);
}

std::string PayloadHashCrc32c::final()
{
    auto val = folly::Endian::big(crc32Value);
    std::string binHash;
    binHash.resize(sizeof(val));
    memcpy(&binHash[0], &val, sizeof(val));
    return binHash;
}

void PayloadHashCrc32c::update(const unsigned char* data, const size_t len)
{
    crc32Value = aws_checksums_crc32c(data, len, crc32Value);
}

// PayloadHashCrc64Nvme methods
bool PayloadHashCrc64Nvme::checkSize()
{
    return expectedHash.size() == sizeof(uint64_t);
}

std::string PayloadHashCrc64Nvme::final()
{
    auto val = folly::Endian::big(crc64Value);
    std::string binHash;
    binHash.resize(sizeof(val));    
    memcpy(&binHash[0], &val, sizeof(val));
    return binHash;
}

void PayloadHashCrc64Nvme::update(const unsigned char* data, const size_t len)
{
    crc64Value = aws_checksums_crc64nvme(data, len, crc64Value);
}


std::unique_ptr<PayloadHashBase> createPayloadHash(const std::string_view& awsHashType)
{
    XLOGF(DBG0, "Creating payload hash for AWS hash type: {}", awsHashType);
    if(awsHashType == "x-amz-checksum-sha256")
    {
        auto hash = std::make_unique<PayloadHashSha256>();
        if(hash->init())
            return hash;
    }
    else if(awsHashType == "x-amz-checksum-crc32")
    {
        auto hash = std::make_unique<PayloadHashCrc32>();
        return hash;
    }
    else if(awsHashType == "x-amz-checksum-crc32c")
    {
        auto hash = std::make_unique<PayloadHashCrc32c>();
        return hash;
    }
    else if(awsHashType == "x-amz-checksum-crc64nvme")
    {
        auto hash = std::make_unique<PayloadHashCrc64Nvme>();
        return hash;
    }
    else if(awsHashType == "x-amz-checksum-sha1")
    {
        auto hash = std::make_unique<PayloadHashSha1>();
        if(hash->init())
            return hash;
    }
    else
    {
        XLOGF(WARNING, "Unsupported AWS hash type requested: {}", awsHashType);
    }

    return nullptr;
}