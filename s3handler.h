#pragma once

#include "SingleFileStorage.h"
#include <condition_variable>
#include <memory>
#include <proxygen/httpserver/HTTPServer.h>
#include <proxygen/httpserver/RequestHandler.h>
#include <proxygen/httpserver/RequestHandlerFactory.h>
#include <proxygen/httpserver/ResponseBuilder.h>
#include <proxygen/lib/http/HTTPHeaders.h>
#include <vector>
#include <thread>
#include <expat.h>
#include <openssl/evp.h>

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

    bool init() {
        assert(ctx==nullptr);
        ctx = EVP_MD_CTX_new();
        if(ctx==nullptr)
            return false;
        
        return EVP_DigestInit(ctx, EVP_md5())==1;
    }
    
    ~EvpMdCtx() {
        if(ctx != nullptr)
            EVP_MD_CTX_free(ctx);
    }
};

class S3Handler : public proxygen::RequestHandler
{
    SingleFileStorage &sfs;
	const std::string& root_key;

public:
    S3Handler(SingleFileStorage &sfs, const std::string& root_key, const std::string& serverUrl) : sfs(sfs), self(this), root_key(root_key), serverUrl(serverUrl) {}

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

private:
    void readFile(folly::EventBase *evb);
    void readObject(folly::EventBase *evb, std::shared_ptr<S3Handler> self, int64_t offset);
	void onBodyCPU(folly::EventBase *evb, int64_t offs, std::unique_ptr<folly::IOBuf> body);
    void listObjects(proxygen::HTTPMessage& headers, const std::string& bucket);
    void getCommitObject(proxygen::HTTPMessage& headers);
    void getObject(proxygen::HTTPMessage& headers);
    void putObject(proxygen::HTTPMessage& headers);
    void putObjectPart(proxygen::HTTPMessage& headers, int partNumber, int64_t uploadId, std::string uploadVerId);
    void commit(proxygen::HTTPMessage& headers);
    void deleteObject(proxygen::HTTPMessage& headers);

    void listObjects(folly::EventBase *evb, std::shared_ptr<S3Handler> self, const std::string& marker, int max_keys, const std::string& prefix, const std::string& delimiter, const std::string& bucket);
    void createMultipartUpload(proxygen::HTTPMessage& headers);
    void finalizeMultipartUpload();
    bool parseMultipartInfo(const std::string& md5sum, int64_t& totalLen);
    std::string getEtag(const std::string& md5sum);
    int seekMultipartExt(int64_t offset);
    int readNextMultipartExt(int64_t offset);
    int finalizeMultiPart();
    int readMultipartExt(int64_t offset);

	enum class RequestType
	{
		Unknown,
		GetObject,
		HeadObject,
		PutObject,
        DeleteObject,
        ListObjects,
        CompleteMultipartUpload
	};

	std::shared_ptr<S3Handler> self;
	RequestType request_type = RequestType::Unknown;

    std::string fpath;
    std::atomic<bool> paused_{ false };
    int64_t done_bytes = 0;
	bool running = false;
    bool finished_ = false;
	std::atomic<int64_t> put_remaining = -1;
    ExpatXmlParser xmlBody;

    std::string serverUrl;

    std::unique_ptr<MultiPartUploadData> multiPartUploadData;
    std::unique_ptr<MultiPartDownloadData> multiPartDownloadData;

	std::mutex extents_mutex;
	std::condition_variable extents_cond;

	std::vector<SingleFileStorage::Ext> extents;
    EvpMdCtx evpMdCtx;
};
