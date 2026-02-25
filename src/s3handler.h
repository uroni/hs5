#pragma once

#include "SingleFileStorage.h"
#include "Buckets.h"
#include "Policy.h"
#include "Action.h"
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
#include "data.h"
#include "ContentType.h"
#include "PayloadHash.h"
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
    bool needsFinalize = false;
    std::vector<PartExt> exts;
};

struct ParsePartRes
{
    std::string nonce;
    ContentType contentType;
    std::vector<MultiPartDownloadData::PartExt> parts;
};

ParsePartRes parsePartExtsFromStr(const std::string& rdata);
std::vector<MultiPartDownloadData::PartExt> parsePartExts(CRData& rdata);
std::vector<MultiPartDownloadData::PartExt> addPartExt(std::vector<MultiPartDownloadData::PartExt> parts, const int64_t partSize, const int partNum);
std::string serializePartExts(const ParsePartRes& partRes);

std::string make_key(const std::string_view key, const int64_t bucketId, const int64_t version);
std::string make_key(const KeyInfo& keyInfo);
KeyInfoView extractKeyInfoView(const std::string_view key);

const unsigned char metadata_object = 0;
const unsigned char metadata_multipart_object = 1;
const unsigned char metadata_tombstone = 2;
const unsigned char metadata_flag_with_content_type = 4;
const unsigned char metadata_known_flags = metadata_flag_with_content_type;

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

    struct UploadIdDec
    {
        int64_t id;
        std::string nonce;
    };

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
        UploadIdDec uploadId;
        
        struct PartData
        {
            std::string etag;
            int partNumber = 0;
        };

        std::vector<PartData> parts;
    };

    struct DeleteObjectData
    {
        std::string etag;
        std::string key;
        std::optional<int64_t> lastModified = std::nullopt;
        std::optional<int64_t> size = std::nullopt;
        std::optional<int64_t> versionId = std::nullopt;
    };

    struct DeleteObjectsData
    {
        enum class ParseState
        {
            Init,
            InRoot,
            InObject,
            InEtag,
            InKey,
            InLastModified,
            InSize,
            InVersionId,
            InQuiet,
            Unknown,
            InUnknownObjectAttr,
            Finished,
            Invalid
        };

        ParseState parseState = ParseState::Init;
        std::vector<DeleteObjectData> objects;
        bool quiet = false;
        std::string accessKey;
    };

    static bool parseMultipartInfo(const std::string& md5sum, int64_t& totalLen, std::unique_ptr<MultiPartDownloadData>& multiPartDownloadData, ContentType* contentType);
    static std::string getEtag(const std::string& md5sum);
    
    static int seekMultipartExt(SingleFileStorage& sfs, int64_t offset, int64_t bucketId, MultiPartDownloadData& multiPartDownloadData, std::vector<SingleFileStorage::Ext>& extents);
    static int seekMultipart(SingleFileStorage& sfs, int partNumber, int64_t bucketId, const bool addReading, MultiPartDownloadData& multiPartDownloadData, 
            std::vector<SingleFileStorage::Ext>& extents, std::string& etag, int64_t& offset, int64_t& partLen);
    static int readMultipartExt(SingleFileStorage& sfs, int64_t offset, int64_t bucketId, const bool addReading,
         MultiPartDownloadData& multiPartDownloadData, std::vector<SingleFileStorage::Ext>& extents, std::string& etag);
    static int readNextMultipartExt(SingleFileStorage& sfs, int64_t offset, int64_t bucketId, MultiPartDownloadData& multiPartDownloadData, std::vector<SingleFileStorage::Ext>& extents);
    static int finalizeMultiPart(SingleFileStorage& sfs, const std::string& fn, const int64_t bucketId, MultiPartDownloadData& multiPartDownloadData, std::vector<SingleFileStorage::Ext>& extents);

    static std::vector<SingleFileStorage::SFragInfo> onDeleteCallback(const std::string& fn, const std::string& md5sum);
    static std::optional<std::string> onModifyCallback(const std::string& fn, std::string md5sum, std::string md5sumParam);
    static void onMatchCallback(SingleFileStorage::MatchInfo& matchInfo, const SingleFileStorage::SFragInfo& fragInfo, const std::optional<std::string>& etagOverride);

private:
    void readFile(folly::EventBase *evb);
    void readObject(folly::EventBase *evb, std::shared_ptr<S3Handler> self, int64_t offset);
    void onBodyChunked(std::unique_ptr<folly::IOBuf> body);
	void onBodyCPU(folly::EventBase *evb, int64_t offs, std::unique_ptr<folly::IOBuf> body);
    void listObjects(proxygen::HTTPMessage& headers, const std::string& bucket, const bool partial);
    void listObjectsV2(proxygen::HTTPMessage& headers, const std::string& bucket, const int64_t bucketId, const bool partial);
    void listBuckets(proxygen::HTTPMessage& headers, std::string accessKey);
    void listBuckets(folly::EventBase *evb, std::shared_ptr<S3Handler> self, std::string accessKey);
    void getBucketLocation(proxygen::HTTPMessage& headers, const std::string& bucket);
    void getCommitObject(proxygen::HTTPMessage& headers);
    void getObject(proxygen::HTTPMessage& headers, const std::string& accessKey);
    void putObject(proxygen::HTTPMessage& headers);
    void putObjectPart(proxygen::HTTPMessage& headers, int partNumber, int64_t uploadId, std::string uploadVerId);
    void manualCommit(proxygen::HTTPMessage& headers);
    void deleteObject(proxygen::HTTPMessage& headers);
    void abortMultipartUpload(proxygen::HTTPMessage& headers, const std::string& uploadIdStr);
    void deleteBucket(proxygen::HTTPMessage& headers);
    bool commit();

    void listObjects(folly::EventBase *evb, std::shared_ptr<S3Handler> self, const std::string& continuationToken, 
        const int maxKeys, const std::optional<std::string>& prefix, const std::optional<std::string>& startAfter, const std::string& delimiter, const int64_t bucket,
        const bool listV2, const std::string& bucketName, const bool partial, const std::string& markerVersionStr);
    void createMultipartUpload(proxygen::HTTPMessage& headers);
    void finalizeMultipartUpload();
    void finalizeCreateBucket();
    std::string getEtagParsedMultipart(const std::string& md5sum);
    
    void readBodyThread(folly::EventBase *evb);
    void startReadBodyThread(folly::EventBase *evb);
    bool setKeyInfoFromPath(const std::string_view path);
    std::optional<std::string> initPayloadHash(proxygen::HTTPMessage& message);
    void deleteObjects();

    UploadIdDec decryptUploadId(const std::string& encdata);

    std::string fullKeyPath() const;

    static void addReadingMultipartObject(const std::string& key);
    static void removeReadingMultipartObject(const std::string& key, SingleFileStorage& sfs);
    static bool isReadingMultipartObjectAndMarkDel(const std::string& key, const int64_t delUploadId, const int64_t delBucketId, const std::vector<MultiPartDownloadData::PartExt>& delParts);
    void stopChecks();
    void parseMatchInfo(proxygen::HTTPMessage& headers, const bool ifModifiedSince, const bool ifUnmodifiedSince);
    int checkMatchInfo();

	std::shared_ptr<S3Handler> self;
	Action request_action = Action::Unknown;

    KeyInfo keyInfo;
    std::atomic<bool> paused_{ false };
    int64_t done_bytes = 0;
	bool running = false;
    bool finished_ = false;
	std::atomic<int64_t> put_remaining = -1;
    ExpatXmlParser xmlBody;
    std::string bodyData;

    std::string serverUrl;

    std::unique_ptr<SingleFileStorage::MatchInfo> matchInfo;
    std::unique_ptr<MultiPartUploadData> multiPartUploadData;
    std::unique_ptr<MultiPartDownloadData> multiPartDownloadData;
    std::unique_ptr<DeleteObjectsData> deleteObjectsData;
    ContentType contentType;

	std::mutex extents_mutex;
	std::condition_variable extents_cond;

	std::vector<SingleFileStorage::Ext> extents;
    bool extentsInitialized = false;
    EvpMdCtx evpMdCtx;
    std::unique_ptr<PayloadHashBase> payloadHash;

    struct BodyObj
    {
        int64_t offset;
        std::unique_ptr<folly::IOBuf> body;
        bool unpause;
    };

    std::mutex bodyMutex;
    bool hasBodyThread = false;
    std::queue<BodyObj> bodyQueue;

    struct AwsChunkedEncoding
    {
        enum class State
        {
            SizeCr,
            SizeLf,
            Data,
            DataEnd,
            TrailerCr,
            TrailerLf,
            Finished,
            Failed
        };

        bool isSigned = false;
        std::string header;
        State state = State::SizeCr;
        size_t chunkSize = 0;     
        EvpMdCtx evpMdCtx;
        std::string stringToSignHeader;
        std::string signingKey;
        std::string currSig;
        std::string expectedSig;
    };

    std::unique_ptr<AwsChunkedEncoding> awsChunkedEncoding;

    struct ReadingMultipartObject
    {
        size_t refs = 0;
        int64_t delUploadId = -1;
        int64_t delBucketId = -1;
        std::vector<MultiPartDownloadData::PartExt> delParts;
    };

    static std::mutex readingMultipartObjectsMutex;
    static std::unordered_map<std::string, ReadingMultipartObject> readingMultipartObjects;
};
