/**
 * Copyright Martin Raiber. All Rights Reserved.
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */
#include "PayloadHash.h"
#include <aws/checksums/crc.h>
#include <cstring>
#include <folly/String.h>
#include <folly/base64.h>
#include <folly/lang/Bits.h>
#include <folly/logging/xlog.h>
#include <openssl/sha.h>

bool PayloadHashBase::isFinalExpected()
{
    const auto gotHash = final();
    const bool ok = expectedHash == gotHash;
    if (ok)
    {
        XLOGF(INFO, "Payload hash {} ok", folly::hexlify(expectedHash));
    }
    else
    {
        XLOGF(INFO, "Payload hash mismatch, expected {}, got {}", folly::hexlify(expectedHash), folly::hexlify(gotHash));
    }
    return ok;
}

bool PayloadHashBase::isFinalCombinedExpected(const uint64_t numParts)
{
    const auto gotHash = finalCombined(numParts);
    const bool ok = expectedHash == gotHash;
    if (ok)
    {
        XLOGF(INFO, "Payload combined hash {} ok", folly::hexlify(expectedHash));
    }
    else
    {
        XLOGF(INFO, "Payload combined hash mismatch, expected {}, got {}", folly::hexlify(expectedHash), folly::hexlify(gotHash));
    }
    return ok;
}

std::string PayloadHashBase::expectedCombined(const uint64_t numParts) const
{
    if(isFull() || numParts == 0)
        return folly::base64Encode(expectedHash);
    else
        return folly::base64Encode(expectedHash) + "-" + std::to_string(numParts);   
}

// PayloadHashSha256 methods
size_t PayloadHashSha256::getSize() const
{
    return SHA256_DIGEST_LENGTH;
}

std::string PayloadHashSha256::final()
{
    std::string binHash;
    binHash.resize(SHA256_DIGEST_LENGTH);
    EVP_DigestFinal_ex(evpMdCtx.ctx, reinterpret_cast<unsigned char *>(&binHash[0]), nullptr);
    return binHash;
}

void PayloadHashSha256::update(const unsigned char *data, const size_t len)
{
    EVP_DigestUpdate(evpMdCtx.ctx, data, len);
}

bool PayloadHashSha256::init()
{
    return evpMdCtx.init(EVP_sha256());
}

// PayloadHashSha1 methods
const size_t SHA1_DIGEST_LENGTH = 20;

size_t PayloadHashSha1::getSize() const
{
    return SHA1_DIGEST_LENGTH;
}

std::string PayloadHashSha1::final()
{
    std::string binHash;
    binHash.resize(SHA1_DIGEST_LENGTH);
    EVP_DigestFinal_ex(evpMdCtx.ctx, reinterpret_cast<unsigned char *>(&binHash[0]), nullptr);
    return binHash;
}

void PayloadHashSha1::update(const unsigned char *data, const size_t len)
{
    EVP_DigestUpdate(evpMdCtx.ctx, data, len);
}

bool PayloadHashSha1::init()
{
    return evpMdCtx.init(EVP_sha1());
}

// PayloadHashCrc32 methods
size_t PayloadHashCrc32::getSize() const
{
    return sizeof(uint32_t);
}

std::string PayloadHashCrc32::final()
{
    auto val = folly::Endian::big(crc32Value);
    std::string binHash;
    binHash.resize(sizeof(val));
    memcpy(&binHash[0], &val, sizeof(val));
    return binHash;
}

void PayloadHashCrc32::update(const unsigned char *data, const size_t len)
{
    crc32Value = aws_checksums_crc32(data, len, crc32Value);
}

void PayloadHashCrc32Full::combine(const PayloadHashBase &other, const uint64_t len)
{
    if (other.getHashType() != AwsHashType::Crc32 || other.getExpectedHash().size() != sizeof(uint32_t))
    {
        XLOGF(ERR, "Attempting to combine incompatible hash types: {} and {}", static_cast<int>(getHashType()), static_cast<int>(other.getHashType()));
        abort();
    }
    const auto otherHash = other.getExpectedHash();
    uint32_t otherCrc;
    memcpy(&otherCrc, otherHash.data(), sizeof(otherCrc));
    otherCrc = folly::Endian::big(otherCrc);
    crc32Value = aws_checksums_crc32_combine(crc32Value, otherCrc, len);
}

// PayloadHashCrc32c methods
size_t PayloadHashCrc32c::getSize() const
{
    return sizeof(uint32_t);
}

std::string PayloadHashCrc32c::final()
{
    auto val = folly::Endian::big(crc32Value);
    std::string binHash;
    binHash.resize(sizeof(val));
    memcpy(&binHash[0], &val, sizeof(val));
    return binHash;
}

void PayloadHashCrc32c::update(const unsigned char *data, const size_t len)
{
    crc32Value = aws_checksums_crc32c(data, len, crc32Value);
}

void PayloadHashCrc32cFull::combine(const PayloadHashBase &other, const uint64_t len)
{
    if (other.getHashType() != AwsHashType::Crc32c || other.getExpectedHash().size() != sizeof(uint32_t))
    {
        XLOGF(ERR, "Attempting to combine incompatible hash types: {} and {}", static_cast<int>(getHashType()), static_cast<int>(other.getHashType()));
        abort();
    }
    const auto otherHash = other.getExpectedHash();
    uint32_t otherCrc;
    memcpy(&otherCrc, otherHash.data(), sizeof(otherCrc));
    otherCrc = folly::Endian::big(otherCrc);
    crc32Value = aws_checksums_crc32c_combine(crc32Value, otherCrc, len);
}

// PayloadHashCrc64Nvme methods
size_t PayloadHashCrc64Nvme::getSize() const
{
    return sizeof(uint64_t);
}

std::string PayloadHashCrc64Nvme::final()
{
    auto val = folly::Endian::big(crc64Value);
    std::string binHash;
    binHash.resize(sizeof(val));
    memcpy(&binHash[0], &val, sizeof(val));
    return binHash;
}

void PayloadHashCrc64Nvme::update(const unsigned char *data, const size_t len)
{
    crc64Value = aws_checksums_crc64nvme(data, len, crc64Value);
}

void PayloadHashCrc64NvmeFull::combine(const PayloadHashBase &other, const uint64_t len)
{
    if (other.getHashType() != AwsHashType::Crc64Nvme || other.getExpectedHash().size() != sizeof(uint64_t))
    {
        XLOGF(ERR, "Attempting to combine incompatible hash types: {} and {}", static_cast<int>(getHashType()), static_cast<int>(other.getHashType()));
        abort();
    }
    const auto otherHash = other.getExpectedHash();
    uint64_t otherCrc;
    memcpy(&otherCrc, otherHash.data(), sizeof(otherCrc));
    otherCrc = folly::Endian::big(otherCrc);
    crc64Value = aws_checksums_crc64nvme_combine(crc64Value, otherCrc, len);
}

std::unique_ptr<PayloadHashBase> createPayloadHash(const std::string_view &awsHashType)
{
    XLOGF(DBG0, "Creating payload hash for AWS hash type: {}", awsHashType);
    if (awsHashType == "x-amz-checksum-sha256")
    {
        auto hash = std::make_unique<PayloadHashSha256>();
        if (hash->init())
            return hash;
    }
    else if (awsHashType == "x-amz-checksum-crc32")
    {
        auto hash = std::make_unique<PayloadHashCrc32>();
        return hash;
    }
    else if (awsHashType == "x-amz-checksum-crc32c")
    {
        auto hash = std::make_unique<PayloadHashCrc32c>();
        return hash;
    }
    else if (awsHashType == "x-amz-checksum-crc64nvme")
    {
        auto hash = std::make_unique<PayloadHashCrc64Nvme>();
        return hash;
    }
    else if (awsHashType == "x-amz-checksum-sha1")
    {
        auto hash = std::make_unique<PayloadHashSha1>();
        if (hash->init())
            return hash;
    }
    else
    {
        XLOGF(WARNING, "Unsupported AWS hash type requested: {}", awsHashType);
    }

    return nullptr;
}

std::unique_ptr<PayloadHashBase> createSdkChecksum(const std::string_view &awsSdkChecksumType, const bool compositeChecksum)
{
    XLOGF(DBG0, "Creating SDK checksum for AWS SDK checksum type: {}", awsSdkChecksumType);
    if (awsSdkChecksumType == "CRC32" || awsSdkChecksumType == "crc32")
    {
        return compositeChecksum ? std::make_unique<PayloadHashCrc32>() : std::make_unique<PayloadHashCrc32Full>();
    }
    else if (awsSdkChecksumType == "CRC32C" || awsSdkChecksumType == "crc32c")
    {
        return compositeChecksum ? std::make_unique<PayloadHashCrc32c>() : std::make_unique<PayloadHashCrc32cFull>();
    }
    else if (awsSdkChecksumType == "CRC64NVME" || awsSdkChecksumType == "crc64nvme")
    {
        return compositeChecksum ? std::make_unique<PayloadHashCrc64Nvme>() : std::make_unique<PayloadHashCrc64NvmeFull>();
    }
    else if (awsSdkChecksumType == "SHA1" || awsSdkChecksumType == "sha1")
    {
        if (!compositeChecksum)
        {
            XLOGF(WARNING, "Non-composite checksum requested for unsupported AWS SDK checksum type: {}", awsSdkChecksumType);
            return nullptr;
        }
        auto hash = std::make_unique<PayloadHashSha1>();
        if (hash->init())
            return hash;
    }
    else if (awsSdkChecksumType == "SHA256" || awsSdkChecksumType == "sha256")
    {
        if (!compositeChecksum)
        {
            XLOGF(WARNING, "Non-composite checksum requested for unsupported AWS SDK checksum type: {}", awsSdkChecksumType);
            return nullptr;
        }
        auto hash = std::make_unique<PayloadHashSha256>();
        if (hash->init())
            return hash;
    }
    else
    {
        XLOGF(WARNING, "Unsupported AWS SDK checksum type requested: {}", awsSdkChecksumType);
    }

    return nullptr;
}

std::unique_ptr<PayloadHashBase> createPayloadHash(const AwsHashType hashType)
{
    switch (hashType)
    {
        case AwsHashType::None:
            return nullptr;
        case AwsHashType::Sha256:
        {
            auto hash = std::make_unique<PayloadHashSha256>();
            if (hash->init())
                return hash;
        }
        break;
        case AwsHashType::Sha1:
        {
            auto hash = std::make_unique<PayloadHashSha1>();
            if (hash->init())
                return hash;
        }
        break;
        case AwsHashType::Crc32:
            return std::make_unique<PayloadHashCrc32>();
        case AwsHashType::Crc32Full:
            return std::make_unique<PayloadHashCrc32Full>();
        case AwsHashType::Crc32c:
            return std::make_unique<PayloadHashCrc32c>();
        case AwsHashType::Crc32cFull:
            return std::make_unique<PayloadHashCrc32cFull>();
        case AwsHashType::Crc64Nvme:
            return std::make_unique<PayloadHashCrc64Nvme>();
        case AwsHashType::Crc64NvmeFull:
            return std::make_unique<PayloadHashCrc64NvmeFull>();
        default:
            XLOGF(WARN, "Unsupported AWS hash type requested: {}", static_cast<int>(hashType));
            return nullptr;
    }

    XLOGF(WARN, "Failed to initialize hash for AWS hash type: {}", static_cast<int>(hashType));
    return nullptr;
}

void serializeSdkChecksum(const std::string_view hashVal, const AwsHashType hashType, CWData &wdata)
{
    wdata.addInt64(static_cast<int64_t>(hashType));
    wdata.addBuffer(hashVal.data(), hashVal.size());
}

bool deserializeSdkChecksum(CRData &rdata, std::unique_ptr<PayloadHashBase> &hash, const bool withExpectedHash)
{
    int64_t hashTypeInt;
    if (!rdata.getInt64(&hashTypeInt))
        return false;
    const auto hashType = static_cast<AwsHashType>(hashTypeInt);
    hash = createPayloadHash(hashType);
    if (!hash)
        return false;
    if (!withExpectedHash)
        return true;
    if (rdata.getLeft() < hash->getSize())
        return false;

    const auto hashVal = std::string_view(rdata.getCurrDataPtr(), hash->getSize());
    hash->setExpectedHash(hashVal);
    rdata.incrementPtr(hash->getSize());
    return true;
}