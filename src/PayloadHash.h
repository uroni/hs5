#pragma once
#include <string>
#include <memory>
#include <openssl/evp.h>
#include "data.h"

enum class AwsHashType
{
    None = 0,
    Crc32 = 1,
    Crc32c = 2,
    Crc64Nvme = 3,
    Sha1 = 4,
    Sha256 = 5,
    Crc32Full = 6,
    Crc32cFull = 7,
    Crc64NvmeFull = 8
};

class EvpMdCtx
{
public:
    EVP_MD_CTX* ctx = nullptr;

    bool init(const EVP_MD *type) {
        assert(ctx==nullptr);
        ctx = EVP_MD_CTX_new();
        if(ctx==nullptr)
            return false;
        
        return EVP_DigestInit(ctx, type)==1;
    }
    
    ~EvpMdCtx() {
        if(ctx != nullptr)
            EVP_MD_CTX_free(ctx);
    }
};

class PayloadHashBase
{
protected:
    std::string expectedHash;

public:
    virtual ~PayloadHashBase() = default;
    
    void setExpectedHash(const std::string_view hash)
    {
        expectedHash = hash;
    }

    const std::string& getExpectedHash() const
    {
        return expectedHash;
    }

    virtual bool checkSize() const {
        return expectedHash.size() == getSize();
    }
    virtual void update(const unsigned char* data, const size_t len) = 0;
    virtual std::string final() = 0;
    virtual AwsHashType getHashType() const = 0;
    virtual std::string_view getHeaderKey() const = 0;
    virtual size_t getSize() const = 0;

    virtual void combine(const PayloadHashBase& other, const uint64_t len) {
        const auto otherHash = other.getExpectedHash();
        update(reinterpret_cast<const unsigned char*>(otherHash.data()), otherHash.size());
    }

    virtual std::string finalCombined(const uint64_t numParts)
    {
        return final() + "-" + std::to_string(numParts);
    }

    virtual bool canCombine(const PayloadHashBase& other) const
    {
        return getHashType() == other.getHashType();
    }

    bool isFinalExpected();
    bool isFinalCombinedExpected(const uint64_t numParts);

    virtual bool isFull() const
    {
        return false;
    }
};

class PayloadHashSha256 : public PayloadHashBase
{
     EvpMdCtx evpMdCtx;
public:
        uint32_t crc32Value;

    public:
        bool init();
        std::string final() override;
        void update(const unsigned char* data, const size_t len) override;  
        AwsHashType getHashType() const override { return AwsHashType::Sha256; }
        std::string_view getHeaderKey() const override { return "x-amz-checksum-sha256"; }
        size_t getSize() const override;
    };

class PayloadHashSha1 : public PayloadHashBase
{
     EvpMdCtx evpMdCtx;
public:
        uint32_t crc32Value;

    public:
        bool init();
        std::string final() override;
        void update(const unsigned char* data, const size_t len) override;
        AwsHashType getHashType() const override { return AwsHashType::Sha1; }
        std::string_view getHeaderKey() const override { return "x-amz-checksum-sha1"; }
        size_t getSize() const override;
    };

class PayloadHashCrc32 : public PayloadHashBase
{
protected:
    uint32_t crc32Value = 0;

public:
    std::string final() override;
    void update(const unsigned char* data, const size_t len) override;
    AwsHashType getHashType() const override { return AwsHashType::Crc32; }
    std::string_view getHeaderKey() const override { return "x-amz-checksum-crc32"; }
    size_t getSize() const override;
};

class PayloadHashCrc32Full: public PayloadHashCrc32
{
public:
    AwsHashType getHashType() const override { return AwsHashType::Crc32Full; }
    void combine(const PayloadHashBase& other, const uint64_t len) override;
    virtual std::string finalCombined(const uint64_t numParts) override
    {
        return final();
    }
    bool canCombine(const PayloadHashBase& other) const override
    {
        return other.getHashType() == AwsHashType::Crc32 || other.getHashType() == AwsHashType::Crc32Full;
    }

    bool isFull() const override
    {
        return true;
    }
};

class PayloadHashCrc32c : public PayloadHashBase
{
protected:
    uint32_t crc32Value = 0;

public:
    std::string final() override;
    void update(const unsigned char* data, const size_t len) override;
    AwsHashType getHashType() const override { return AwsHashType::Crc32c; }
    std::string_view getHeaderKey() const override { return "x-amz-checksum-crc32c"; }
    size_t getSize() const override;
};

class PayloadHashCrc32cFull : public PayloadHashCrc32c
{
public:
    AwsHashType getHashType() const override { return AwsHashType::Crc32cFull; }
    void combine(const PayloadHashBase& other, const uint64_t len) override;
    virtual std::string finalCombined(const uint64_t numParts) override
    {
        return final();
    }
    bool canCombine(const PayloadHashBase& other) const override
    {
        return other.getHashType() == AwsHashType::Crc32c || other.getHashType() == AwsHashType::Crc32cFull;
    }

    bool isFull() const override
    {
        return true;
    }
};

class PayloadHashCrc64Nvme : public PayloadHashBase
{
protected:
    uint64_t crc64Value = 0;

public:
    std::string final() override;
    void update(const unsigned char* data, const size_t len) override;
    AwsHashType getHashType() const override { return AwsHashType::Crc64Nvme; }
    std::string_view getHeaderKey() const override { return "x-amz-checksum-crc64nvme"; }
    size_t getSize() const override;
};

class PayloadHashCrc64NvmeFull : public PayloadHashCrc64Nvme
{
public:
    AwsHashType getHashType() const override { return AwsHashType::Crc64NvmeFull; }
    void combine(const PayloadHashBase& other, const uint64_t len) override;
    virtual std::string finalCombined(const uint64_t numParts) override
    {
        return final();
    }
    bool canCombine(const PayloadHashBase& other) const override
    {
        return other.getHashType() == AwsHashType::Crc64Nvme || other.getHashType() == AwsHashType::Crc64NvmeFull;
    }

    bool isFull() const override
    {
        return true;
    }
};

std::unique_ptr<PayloadHashBase> createPayloadHash(const std::string_view& awsHashType);

std::unique_ptr<PayloadHashBase> createSdkChecksum(const std::string_view& awsSdkChecksumType, const bool compositeChecksum);

std::unique_ptr<PayloadHashBase> createPayloadHash(const AwsHashType hashType);

void serializeSdkChecksum(const std::string_view hashVal, const AwsHashType hashType, CWData& wdata);

bool deserializeSdkChecksum(CRData& rdata, std::unique_ptr<PayloadHashBase>& hash, const bool withExpectedHash);
