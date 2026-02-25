#pragma once
#include <string>
#include <memory>
#include <openssl/evp.h>

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
    
    void setExpectedHash(const std::string& hash)
    {
        expectedHash = hash;
    }

    virtual bool checkSize() = 0;
    virtual void update(const unsigned char* data, const size_t len) = 0;
    virtual std::string final() = 0;

    bool isFinalExpected();
};

class PayloadHashSha256 : public PayloadHashBase
{
     EvpMdCtx evpMdCtx;
public:
        uint32_t crc32Value;

    public:
        bool init();
        bool checkSize() override;
        std::string final() override;
        void update(const unsigned char* data, const size_t len) override;  
    };

class PayloadHashSha1 : public PayloadHashBase
{
     EvpMdCtx evpMdCtx;
public:
        uint32_t crc32Value;

    public:
        bool init();
        bool checkSize() override;
        std::string final() override;
        void update(const unsigned char* data, const size_t len) override;

        
    };

class PayloadHashCrc32 : public PayloadHashBase
{
private:
    uint32_t crc32Value = 0;

public:
    bool checkSize() override;
    std::string final() override;
    void update(const unsigned char* data, const size_t len) override;
};


class PayloadHashCrc32c : public PayloadHashBase
{
private:
    uint32_t crc32Value = 0;

public:
    bool checkSize() override;
    std::string final() override;
    void update(const unsigned char* data, const size_t len) override;
};

class PayloadHashCrc64Nvme : public PayloadHashBase
{
private:
    uint64_t crc64Value = 0;

public:
    bool checkSize() override;
    std::string final() override;
    void update(const unsigned char* data, const size_t len) override;
};

std::unique_ptr<PayloadHashBase> createPayloadHash(const std::string_view& awsHashType);