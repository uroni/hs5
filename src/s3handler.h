#pragma once

#include "SingleFileStorage.h"
#include "Buckets.h"
#include <condition_variable>
#include <memory>
#include <proxygen/httpserver/HTTPServer.h>
#include <proxygen/httpserver/RequestHandler.h>
#include <proxygen/httpserver/RequestHandlerFactory.h>
#include <proxygen/httpserver/ResponseBuilder.h>
#include <proxygen/lib/http/HTTPHeaders.h>
#include <proxygen/lib/http/HTTPMessage.h>
#include <proxygen/lib/http/ProxygenErrorEnum.h>
#include <vector>
#include <thread>
#include <expat.h>
#include <openssl/evp.h>

#include "ApiHandler.h"

class ExpatXmlParser
{
public:
    XML_Parser parser = nullptr;

    void init() {
        parser = XML_ParserCreate("UTF-8");
    }

    ~ExpatXmlParser()
    {
        if(parser!=nullptr)
            XML_ParserFree(parser);
    }
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

int	mdb_cmp_s3key(const MDB_val *a, const MDB_val *b);
std::string s3key_common_prefix(const std::string_view key);
size_t s3key_common_prefix_hash(const std::string_view key);

struct KeyInfo
{
    std::string key;
    int64_t version;
    int64_t bucketId;
};

struct KeyInfoView
{
    std::string_view key;
    int64_t version;
    int64_t bucketId;
};

std::string make_key(const std::string_view key, const int64_t bucketId, const int64_t version);
std::string make_key(const KeyInfo& keyInfo);
KeyInfoView extractKeyInfoView(const std::string_view key);

const char metadata_object = 0;
const char metadata_multipart_object = 1;
const char metadata_tombstone = 2;

class S3Handler : public proxygen::RequestHandler
{
public:
    const bool withBucketVersioning;
    SingleFileStorage &sfs;

    S3Handler(SingleFileStorage &sfs, const std::string& serverUrl, bool withBucketVersioning) : sfs(sfs), self(this), serverUrl(serverUrl), withBucketVersioning(withBucketVersioning) {}

    void
    onRequest(std::unique_ptr<proxygen::HTTPMessage> headers) noexcept override;

    void onBody(std::unique_ptr<folly::IOBuf> body) noexcept override;

    void onEOM() noexcept override;

    void onUpgrade(proxygen::UpgradeProtocol proto) noexcept override;

    void requestComplete() noexcept override;

    void onError(proxygen::ProxygenError err) noexcept override;

    void onEgressPaused() noexcept override;

    void onEgressResumed() noexcept override;

    struct MultiPartUploadData
    {
        enum class ParseState
        {
            Init,
            InRoot,
            InPart,
            InPartNumber,
            InEtag,
            InUnknownPartAttr,
            Finished,
            InvalidPartOrder,
            Unknown
        };

        ParseState parseState = ParseState::Init;
        int64_t uploadId;
        std::string verId;
        
        struct PartData
        {
            std::string etag;
            int partNumber = 0;
        };

        std::vector<PartData> parts;
    };

    struct MultiPartDownloadData
    {
        struct PartExt
        {
            int64_t size;
            int start;
            int len;
        };

        std::string etag;
        int64_t uploadId;
        int64_t numParts;
        size_t extIdx = std::string::npos;
        int64_t currOffset = 0;
        PartExt currExt;
        std::vector<PartExt> exts;
    };

    struct PayloadHash
    {
        enum class Method
        {
            Sha256
        };
        Method method;
        std::string expectedHash;
        EvpMdCtx evpMdCtx;

        bool checkSize()
        {
            if(method==Method::Sha256)
                return expectedHash.size() == SHA256_DIGEST_LENGTH;

            return true;
        }

        std::string final()
        {
            std::string binHash;
            switch(method)
            {
                case Method::Sha256:
                    binHash.resize(SHA256_DIGEST_LENGTH);
                    break;
                default:
                    return std::string();
            }
            EVP_DigestFinal_ex(evpMdCtx.ctx, reinterpret_cast<unsigned char*>(&binHash[0]), nullptr);
            return binHash;
        }

        bool isFinalExpected()
        {
            const bool ok = expectedHash == final();
            if(ok)
            {
                XLOGF(INFO, "Payload hash {} ok", folly::hexlify(expectedHash));
            }
            return ok;
        }
    };

private:
    void readFile(folly::EventBase *evb);
    void readObject(folly::EventBase *evb, std::shared_ptr<S3Handler> self, int64_t offset);
	void onBodyCPU(folly::EventBase *evb, int64_t offs, std::unique_ptr<folly::IOBuf> body);
    void listObjects(proxygen::HTTPMessage& headers, const std::string& bucket);
    void listObjectsV2(proxygen::HTTPMessage& headers, const std::string& bucket, const int64_t bucketId);
    void getCommitObject(proxygen::HTTPMessage& headers);
    void getObject(proxygen::HTTPMessage& headers, const std::string& accessKey);
    void putObject(proxygen::HTTPMessage& headers);
    void putObjectPart(proxygen::HTTPMessage& headers, int partNumber, int64_t uploadId, std::string uploadVerId);
    void commit(proxygen::HTTPMessage& headers);
    void deleteObject(proxygen::HTTPMessage& headers);
    void deleteBucket(proxygen::HTTPMessage& headers);

    void listObjects(folly::EventBase *evb, std::shared_ptr<S3Handler> self, const std::string& continuationToken, 
        const int maxKeys, const std::optional<std::string>& prefix, const std::optional<std::string>& startAfter, const std::string& delimiter, const int64_t bucket,
        const bool listV2, const std::string& bucketName);
    void createMultipartUpload(proxygen::HTTPMessage& headers);
    void finalizeMultipartUpload();
    void finalizeCreateBucket();
    bool parseMultipartInfo(const std::string& md5sum, int64_t& totalLen);
    std::string getEtag(const std::string& md5sum);
    int seekMultipartExt(int64_t offset);
    int readNextMultipartExt(int64_t offset);
    int finalizeMultiPart();
    int readMultipartExt(int64_t offset);
    void readBodyThread(folly::EventBase *evb);
    bool setKeyInfoFromPath(const std::string_view path);
    std::optional<std::string> initPayloadHash(proxygen::HTTPMessage& message);

	enum class RequestType
	{
		Unknown,
		GetObject,
		HeadObject,
		PutObject,
        DeleteObject,
        ListObjects,
        CompleteMultipartUpload,
        CreateBucket,
        DeleteBucket
	};

	std::shared_ptr<S3Handler> self;
	RequestType request_type = RequestType::Unknown;

    KeyInfo keyInfo;
    std::atomic<bool> paused_{ false };
    int64_t done_bytes = 0;
	bool running = false;
    bool finished_ = false;
	std::atomic<int64_t> put_remaining = -1;
    ExpatXmlParser xmlBody;
    std::string bodyData;

    std::string serverUrl;

    std::unique_ptr<MultiPartUploadData> multiPartUploadData;
    std::unique_ptr<MultiPartDownloadData> multiPartDownloadData;

	std::mutex extents_mutex;
	std::condition_variable extents_cond;

	std::vector<SingleFileStorage::Ext> extents;
    EvpMdCtx evpMdCtx;
    std::unique_ptr<PayloadHash> payloadHash;

    struct BodyObj
    {
        int64_t offset;
        std::unique_ptr<folly::IOBuf> body;
        bool unpause;
    };

    std::mutex bodyMutex;
    bool hasBodyThread = false;
    std::queue<BodyObj> bodyQueue;
};
