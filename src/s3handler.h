#pragma once

#include "Action.h"
#include "ApiHandler.h"
#include "Buckets.h"
#include "ObjMetadata.h"
#include "PayloadHash.h"
#include "Policy.h"
#include "SingleFileStorage.h"
#include "ThreadSafety.h"
#include "data.h"
#include <atomic>
#include <cassert>
#include <condition_variable>
#include <expat.h>
#include <folly/executors/GlobalExecutor.h>
#include <folly/io/IOBuf.h>
#include <folly/io/async/EventBase.h>
#include <folly/io/async/EventBaseManager.h>
#include <memory>
#include <proxygen/httpserver/HTTPServer.h>
#include <proxygen/httpserver/RequestHandler.h>
#include <proxygen/httpserver/RequestHandlerFactory.h>
#include <proxygen/httpserver/ResponseBuilder.h>
#include <proxygen/lib/http/HTTPHeaders.h>
#include <proxygen/lib/http/HTTPMessage.h>
#include <proxygen/lib/http/ProxygenErrorEnum.h>
#include <proxygen/lib/http/experimental/RFC1867.h>
#include <thread>
#include <type_traits>
#include <vector>

class ExpatXmlParser
{
public:
    XML_Parser parser = nullptr;

    void init()
    {
        parser = XML_ParserCreate("UTF-8");
    }

    ~ExpatXmlParser()
    {
        if (parser != nullptr)
            XML_ParserFree(parser);
    }
};

// Defined in s3handler.cpp
enum class S3ErrorCode;

int mdb_cmp_s3key(const MDB_val* a, const MDB_val* b);
std::string s3key_common_prefix(const std::string_view key);
size_t s3key_common_prefix_hash(const std::string_view key);

struct KeyInfo
{
    std::string key;
    int64_t version;
    int64_t bucketId;
    buckets::VersioningState versioning;

    bool withVersioning() const
    {
        return versioning == buckets::VersioningState::Enabled || versioning == buckets::VersioningState::Suspended;
    }
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
    ObjMetadata objMetadata;
    AwsHashType sdkChecksumType;
    std::vector<MultiPartDownloadData::PartExt> parts;
};

enum class PostObjectState
{
    Init,
    MultipartParsingError,
    Error,
    ParseSuccessActionStatus,
    ParseKey,
    File,
    Done
};

struct PostObjectData
{
    PostObjectData(const std::string& boundary)
        : rfc1867Codec(boundary)
    {
    }

    std::unique_ptr<folly::IOBuf> bodyBuf;
    proxygen::RFC1867Codec rfc1867Codec;
    PostObjectState state = PostObjectState::Init;
    std::optional<std::string> successActionStatus;
    std::optional<std::string> key;
    int64_t expectedLength = 0;
};

ParsePartRes parsePartExtsFromStr(const std::string& rdata);
std::vector<MultiPartDownloadData::PartExt> parsePartExts(CRData& rdata);
std::vector<MultiPartDownloadData::PartExt> addPartExt(std::vector<MultiPartDownloadData::PartExt> parts, const int64_t partSize, const int partNum);
std::string serializePartExts(const ParsePartRes& partRes);

std::string make_key(const std::string_view key, const int64_t bucketId, const int64_t version);
std::string make_key(const KeyInfo& keyInfo);
KeyInfoView extractKeyInfoView(const std::string_view key);

const int64_t metadata_object = 0;
const int64_t metadata_multipart_object = 1;
const int64_t metadata_tombstone = 2;
const int64_t metadata_flag_with_content_type = 1 << 2;
const int64_t metadata_flag_with_meta = 1 << 3;
const int64_t metadata_flag_with_checksum = 1 << 4;
const int64_t metadata_flag_with_cache_control = 1 << 6;
const int64_t metadata_flag_with_encoding = 1 << 7;
const int64_t metadata_flag_with_expires = 1 << 8;
const int64_t metadata_flag_with_language = 1 << 9;
const int64_t metadata_flag_with_disposition = 1 << 10;
const int64_t metadata_flag_with_lock_info = 1 << 11;
const int64_t metadata_flag_with_legal_hold = 1 << 12;
const int64_t metadata_flag_has_obj_metadata = metadata_flag_with_content_type | metadata_flag_with_meta |
                                               metadata_flag_with_disposition | metadata_flag_with_cache_control |
                                               metadata_flag_with_encoding | metadata_flag_with_language | metadata_flag_with_expires |
                                               metadata_flag_with_lock_info | metadata_flag_with_legal_hold;
const int64_t metadata_known_flags = metadata_flag_with_content_type | metadata_flag_with_meta |
                                     metadata_flag_with_disposition | metadata_flag_with_cache_control |
                                     metadata_flag_with_encoding | metadata_flag_with_language |
                                     metadata_flag_with_expires | metadata_flag_with_checksum |
                                     metadata_flag_with_lock_info | metadata_flag_with_legal_hold;

// Modification types for SingleFileStorage::modify_metadata
const char metadata_modify_legal_hold = 1;
const char metadata_modify_retention = 2;

class S3Handler : public proxygen::RequestHandler, public proxygen::RFC1867Codec::Callback
{
public:
    S3Handler(SingleFileStorage& sfs, const std::string& serverUrl) : _sfs(sfs), self(this), serverUrl(serverUrl) {}

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
            InvalidPartNumber,
            Unknown
        };

        ParseState parseState = ParseState::Init;
        UploadIdDec uploadId;
        std::string partNumBuf;

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
        std::string lastModified;
        std::string size;
        std::string versionId;
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
            InStatus,
            Unknown,
            InUnknownObjectAttr,
            Finished,
            Invalid
        };

        ParseState parseState = ParseState::Init;
        std::vector<DeleteObjectData> objects;
        bool quiet = false;
        std::string accessKey;
        std::string buf;
        bool bypassGovernanceRequested = false;
    };

    struct PutObjectRetentionData
    {
        enum class ParseState
        {
            Init,
            InRoot,
            InMode,
            InRetainUntilDate,
            InEventHold,
            InEventHoldDuration,
            InDays,
            InYears,
            Unknown,
            Finished
        };

        ParseState parseState = ParseState::Init;
        std::string mode;
        std::string retainUntilDate;
        std::string eventHold;
        std::string days;
        std::string years;
        bool hasEventHoldDuration = false;
        // x-amz-bypass-governance-retention set and authorized
        bool bypassGovernance = false;
    };

    struct PutObjectLockConfigurationData
    {
        enum class ParseState
        {
            Init,
            InRoot,
            InObjectLockEnabled,
            Unknown,
            Finished
        };

        ParseState parseState = ParseState::Init;
        std::string objectLockEnabled;
        bool hasRule = false;
        // Depth of unknown/ignored elements below the root
        int unknownDepth = 0;
    };

    struct PutBucketCorsData
    {
        enum class ParseState
        {
            Init,
            InRoot,
            InRule,
            InAllowedOrigin,
            InAllowedMethod,
            InAllowedHeader,
            InExposeHeader,
            InMaxAgeSeconds,
            InId,
            Unknown,
            InUnknownObjectAttr,
            Finished,
            Invalid
        };

        ParseState parseState = ParseState::Init;
        std::vector<buckets::BucketCorsRule> rules;
    };

    static bool parseMultipartInfo(const std::string& md5sum, int64_t& totalLen,
                                   std::unique_ptr<MultiPartDownloadData>& multiPartDownloadData, std::unique_ptr<ObjMetadata>* objMetadata, std::unique_ptr<PayloadHashBase>* sdkChecksumHash);
    static std::string getEtag(const std::string& md5sum);

    static int seekMultipartExt(SingleFileStorage& sfs, int64_t offset, int64_t bucketId, MultiPartDownloadData& multiPartDownloadData, std::vector<SingleFileStorage::Ext>& extents);
    static int seekMultipart(SingleFileStorage& sfs, int partNumber, int64_t bucketId, const bool addReading, MultiPartDownloadData& multiPartDownloadData,
                             std::vector<SingleFileStorage::Ext>& extents, std::string& etag, int64_t& offset, int64_t& partLen, std::unique_ptr<PayloadHashBase>& partHash, int64_t* partSize);
    static int readMultipartExt(SingleFileStorage& sfs, int64_t offset, int64_t bucketId, const bool addReading,
                                MultiPartDownloadData& multiPartDownloadData, std::vector<SingleFileStorage::Ext>& extents, std::string* etag, std::unique_ptr<PayloadHashBase>* partHash, int64_t* partSize);
    static int readNextMultipartExt(SingleFileStorage& sfs, int64_t offset, int64_t bucketId, const bool addReading, MultiPartDownloadData& multiPartDownloadData, std::vector<SingleFileStorage::Ext>& extents,
                                    std::string* etag, std::unique_ptr<PayloadHashBase>* partHash, int64_t* partSize);
    static int finalizeMultiPart(SingleFileStorage& sfs, const std::string& fn, const int64_t bucketId, MultiPartDownloadData& multiPartDownloadData, std::vector<SingleFileStorage::Ext>& extents);

    static std::vector<SingleFileStorage::SFragInfo> onDeleteCallback(const std::string& fn, const std::string& md5sum);
    static std::optional<std::string> onModifyCallback(const std::string& fn, std::string md5sum, std::string md5sumParam);
    static nonstd::expected<std::string, int> onModifyMetadataCallback(const std::string& fn, const std::string& md5sum, const std::string& param);
    static void onMatchCallback(SingleFileStorage::MatchInfo& matchInfo, const SingleFileStorage::SFragInfo& fragInfo, const std::optional<std::string>& etagOverride);
    // Object is protected from deletion by legal hold or object lock retention at time 'now'
    static bool isProtected(const std::string_view md5sum, const std::chrono::system_clock::time_point now, const bool bypassGovernance);
    static void onAddReadingCallback(const std::string_view fn, const SingleFileStorage::SFragInfo& fragInfo);

    static std::string md5sumBinFromData(const std::string_view md5sumData);

    static std::pair<int64_t, std::string_view> flagsAndMd5sumBinFromData(const std::string_view md5sumData);

    void sigCheckOk();
    void sigCheckFailed();

    // multipart/form-data parsing callbacks
    virtual int onFieldStart(const std::string& name,
                             folly::Optional<std::string> filename,
                             std::unique_ptr<proxygen::HTTPMessage> msg,
                             uint64_t postBytesProcessed) override;
    virtual int onFieldData(std::unique_ptr<folly::IOBuf>,
                            uint64_t postBytesProcessed) override;
    virtual void onFieldEnd(bool endedOnBoundary,
                            uint64_t postBytesProcessed) override;
    virtual void onError() override;

private:
    void readFile(folly::EventBase* evb);
    void readObject(folly::EventBase* evb, std::shared_ptr<S3Handler> self, int64_t offset, unsigned int readFlags = 0) RELEASE(requestOwner);
    // Helps cleanup a ressource on all paths
    ThreadRole pendingCleanup;

    void beginPendingCleanup() ACQUIRE(pendingCleanup) NO_THREAD_SAFETY_ANALYSIS
    {
    }

    void endPendingCleanup() RELEASE(pendingCleanup) NO_THREAD_SAFETY_ANALYSIS
    {
    }

    // Releases the object read_prepare'd
    void prepareReadCleanup(const std::string& findPath, std::vector<SingleFileStorage::Ext>& extents) REQUIRES(requestOwner) RELEASE(pendingCleanup);

    // Releases the extents read by readObject
    void readFinalize() REQUIRES(requestOwner) RELEASE(pendingCleanup);
    void onBodyChunked(std::unique_ptr<folly::IOBuf> body) REQUIRES(httpThread, requestActive);
    void onBodyCPU(folly::EventBase* evb, std::shared_ptr<S3Handler> self, int64_t offs, std::unique_ptr<folly::IOBuf> body);
    // Sets the hash received in an aws-chunked trailer as expected hash. Returns an error message if that fails
    const char* applyTrailerHash(const std::string& expectedHash) REQUIRES(requestOwner);
    void listObjects(proxygen::HTTPMessage& headers, const std::string& bucket) REQUIRES(httpThread, requestActive);
    void listObjectsV2(proxygen::HTTPMessage& headers, const std::string& bucket, const int64_t bucketId) REQUIRES(httpThread, requestActive);
    void listBuckets(proxygen::HTTPMessage& headers, std::string accessKey) REQUIRES(httpThread, requestActive);
    void listBuckets(folly::EventBase* evb, std::shared_ptr<S3Handler> self, std::string accessKey) RELEASE(requestOwner);
    void getBucketLocation(proxygen::HTTPMessage& headers, const std::string& bucket) REQUIRES(httpThread, requestActive);
    void getBucketVersioning(proxygen::HTTPMessage& headers, const std::string& bucket) REQUIRES(httpThread, requestActive);
    void getBucketCors(proxygen::HTTPMessage& headers, const std::string& bucket) REQUIRES(httpThread, requestActive);
    void getBucketObjectLockConfiguration(proxygen::HTTPMessage& headers, const std::string& bucket) REQUIRES(httpThread, requestActive);
    void getCommitObject(proxygen::HTTPMessage& headers) REQUIRES(httpThread, requestActive);
    void getObject(proxygen::HTTPMessage& headers, const std::string& accessKey) REQUIRES(httpThread, requestActive);
    void getObjectAttributes(proxygen::HTTPMessage& headers, const std::string& accessKey) REQUIRES(httpThread, requestActive);
    void listParts(proxygen::HTTPMessage& headers) REQUIRES(httpThread, requestActive);
    struct CopySource
    {
        std::string bucket;
        std::string key;
        // 0 means newest version
        int64_t version = 0;
    };

    void putObject(proxygen::HTTPMessage& headers, std::optional<CopySource> copySource) REQUIRES(httpThread, requestActive);
    void putObjectPart(proxygen::HTTPMessage& headers, int partNumber, int64_t uploadId, std::string uploadVerId, std::optional<CopySource> copySource) REQUIRES(httpThread, requestActive);
    void manualCommit(proxygen::HTTPMessage& headers) REQUIRES(httpThread, requestActive);
    void deleteObject(proxygen::HTTPMessage& headers, const bool bypassGovernance) REQUIRES(httpThread, requestActive);
    void abortMultipartUpload(proxygen::HTTPMessage& headers, const std::string& uploadIdStr) REQUIRES(httpThread, requestActive);
    void deleteBucket(proxygen::HTTPMessage& headers) REQUIRES(httpThread, requestActive);
    void putBucketVersioning(proxygen::HTTPMessage& headers) REQUIRES(httpThread, requestActive, requestOwner);
    void putBucketCors(proxygen::HTTPMessage& headers) REQUIRES(httpThread, requestActive, requestOwner);
    void putObjectLegalHold(proxygen::HTTPMessage& headers) REQUIRES(httpThread, requestActive, requestOwner);
    void putObjectLockConfiguration(proxygen::HTTPMessage& headers) REQUIRES(httpThread, requestActive, requestOwner);
    void putObjectRetention(proxygen::HTTPMessage& headers, const bool bypassGovernance) REQUIRES(httpThread, requestActive, requestOwner);
    static bool bypassGovernanceRequested(proxygen::HTTPMessage& headers);
    // Returns if governance retention should be bypassed (requested via header).
    // Sends error response and returns std::nullopt if bypass is requested but not authorized
    std::optional<bool> checkBypassGovernance(proxygen::HTTPMessage& headers, const std::string_view resource, const std::string_view accessKey) REQUIRES(httpThread, requestActive);
    // Parses x-amz-object-lock-* headers of object writes into objMetadata.
    // Sends error response and returns false if headers are invalid or not authorized
    bool parseObjectLockHeaders(proxygen::HTTPMessage& headers, const std::string_view resource, const std::string_view accessKey) REQUIRES(httpThread, requestActive, requestOwner);
    void optionsObject(proxygen::HTTPMessage& headers) REQUIRES(httpThread, requestActive, requestOwner);
    void setCorsAllowOrigin(proxygen::HTTPMessage& headers, const std::string_view bucketName) REQUIRES(httpThread, requestActive);
    void setCorsAllowOrigin(proxygen::HTTPMessage& headers, const buckets::BucketInfo& bucketInfo) REQUIRES(httpThread, requestActive);
    bool commit();

    void listObjects(folly::EventBase* evb, std::shared_ptr<S3Handler> self, const std::string& continuationToken,
                     const int maxKeys, const std::optional<std::string>& prefix, const std::optional<std::string>& startAfter, const std::string& delimiter, const int64_t bucket,
                     const bool listV2, const std::string& bucketName, const std::string& markerVersionStr, const bool urlEncode) RELEASE(requestOwner);
    void createMultipartUpload(proxygen::HTTPMessage& headers) REQUIRES(httpThread, requestActive);
    void finalizeMultipartUpload() REQUIRES(httpThread, requestActive);
    void finalizeCreateBucket() REQUIRES(httpThread, requestActive);
    void finalizePutBucketVersioning() REQUIRES(httpThread, requestActive);
    void finalizePutBucketCors() REQUIRES(httpThread, requestActive);
    void finalizePutObjectLegalHold() REQUIRES(httpThread, requestActive);
    void finalizePutObjectLockConfiguration() REQUIRES(httpThread, requestActive);
    void finalizePutObjectRetention() REQUIRES(httpThread, requestActive);
    // Send a response from a CPU task, handing the request back to the HTTP thread
    void sendResponse(folly::EventBase* evb, const std::shared_ptr<S3Handler>& self, const uint16_t code, const std::string& message,
                      std::string body = std::string()) RELEASE(requestOwner) EXCLUDES(pendingCleanup);
    // Send S3 error response
    void sendError(folly::EventBase* evb, const std::shared_ptr<S3Handler>& self, const uint16_t code, const std::string& message,
                   const S3ErrorCode errorCode, const std::string& errorMsg, std::optional<std::string> resource = std::nullopt) RELEASE(requestOwner) EXCLUDES(pendingCleanup);
    std::string getEtagParsedMultipart(const std::string& md5sum) REQUIRES(requestOwner);
    bool computeMd5() const REQUIRES(requestOwner);

    void readBodyThread(folly::EventBase* evb, std::shared_ptr<S3Handler> self) EXCLUDES(bodyMutex);
    bool waitForExtents() EXCLUDES(extents_mutex) ASSERT_CAPABILITY(requestOwner);
    void startReadBodyThread(folly::EventBase* evb) REQUIRES(bodyMutex, httpThread);
    bool usesBodyThread() const;
    void setFinished() REQUIRES(httpThread) EXCLUDES(extents_mutex);
    bool isFinished() const REQUIRES(httpThread)
    {
        return finished_;
    }
    bool setKeyInfoFromPath(proxygen::HTTPMessage& headers, const std::string_view path, const std::string* versionId, const bool setCorsHeader) REQUIRES(httpThread, requestActive, requestOwner);
    std::optional<std::string> initPayloadHash(proxygen::HTTPMessage& message) REQUIRES(httpThread, requestActive, requestOwner);
    bool initSdkChecksum(proxygen::HTTPMessage& message, const Action action) REQUIRES(httpThread, requestActive, requestOwner);
    void deleteObjects() REQUIRES(httpThread, requestActive);

    std::string encryptUploadId(const int64_t id, const std::string& nonce);
    UploadIdDec decryptUploadId(const std::string& encdata);

    std::string fullKeyPath() const REQUIRES(requestOwner);

    static void removeReadingMultipartObject(const std::string& key, SingleFileStorage& sfs);
    static bool isReadingMultipartObjectAndMarkDel(const std::string& key, const int64_t delUploadId, const int64_t delBucketId, const std::vector<MultiPartDownloadData::PartExt>& delParts);
    void stopChecks();
    void parseMatchInfo(proxygen::HTTPMessage& headers, const bool ifModifiedSince, const bool ifUnmodifiedSince) REQUIRES(requestOwner);
    void parseSourceMatchInfo(proxygen::HTTPMessage& headers, SingleFileStorage::MatchInfo& matchInfo);
    // Fail overwrite/delete of the current object if it is protected (legal hold).
    // Only if object lock is enabled for the bucket. Call right before queueing the overwrite/delete.
    void addProtectedCheck(const bool bypassGovernance) REQUIRES(requestOwner);
    int checkMatchInfo() REQUIRES(requestOwner);
    void finalizePutObject(folly::EventBase* evb, std::shared_ptr<S3Handler> self, const int64_t lastModified, const int64_t copySourceVersion) RELEASE(requestOwner);

    bool serializeMetadataValues(CWData& wdata);
    bool unserializeMetadataValues(CRData& rdata);

    void postObject() REQUIRES(httpThread, requestActive);

    // Parses x-amz-copy-source and checks that accessKey may read the source.
    // Sends an error response and returns nullopt otherwise
    std::optional<CopySource> parseCopySource(proxygen::HTTPMessage& headers, const std::string_view accessKey) REQUIRES(httpThread, requestActive);

    struct CopyObjectInfo
    {
        // x-amz-copy-source header value (for logging and error responses)
        std::string source;
        CopySource sourceObject;
        std::string range;
        enum class MetadataDirective
        {
            Copy,
            Replace
        } metadataDirective = MetadataDirective::Copy;
        SingleFileStorage::MatchInfo sourceMatchInfo;
    };

    void copyObject(folly::EventBase* evb, std::shared_ptr<S3Handler> self, const std::string& targetFn, CopyObjectInfo& copyObjectInfo) RELEASE(requestOwner);
    // Frees the extents copyObject prepared for writing
    void freeCopyExtents() REQUIRES(requestOwner) RELEASE(pendingCleanup);

    proxygen::ResponseBuilder responseBuilderWithCors(uint16_t code, const std::string& message) REQUIRES(httpThread, requestActive);

    // Owning reference to this handler while the request is active. Reset in requestComplete/onError.
    // Code on other threads must keep the handler alive with its own copy, handed over from the HTTP thread
    std::shared_ptr<S3Handler> self GUARDED_BY(httpThread);
    Action request_action GUARDED_BY(httpThread) = Action::Unknown;

    Action requestAction() const NO_THREAD_SAFETY_ANALYSIS
    {
        return request_action;
    }

    KeyInfo keyInfo GUARDED_BY(requestOwner);
    std::atomic<bool> paused_{ false };

    // Role held while running on the HTTP thread of this request
    ThreadRole httpThread;

    void assertOnHttpThread(folly::EventBase* evb) const ASSERT_CAPABILITY(httpThread)
    {
        assert(evb != nullptr && evb->isInEventBaseThread());
    }

    void assertOnHttpThread() const ASSERT_CAPABILITY(httpThread)
    {
#ifndef NDEBUG
        const auto evb = folly::EventBaseManager::get()->getExistingEventBase();
        assert(evb != nullptr && evb->isInEventBaseThread());
#endif
    }

    // Role held while the request is active, i.e. before requestComplete/onError. Required for downstream()
    ThreadRole requestActive;

    // Thrown by assertActive/assertNotCompleted, caught by runInHttpThread
    struct RequestInactive
    {
    };

    // Call at the start of proxygen callbacks, which are only called while the request is active
    void assertInCallback() const ASSERT_CAPABILITY(httpThread) ASSERT_CAPABILITY(requestActive)
    {
        assertOnHttpThread();
        assert(self != nullptr);
    }

    // Skips the rest of a runInHttpThread lambda if the request is finished. Not finished implies active, as
    // requestComplete/onError set finished_ before resetting self
    void assertActive() const REQUIRES(httpThread) ASSERT_CAPABILITY(requestActive)
    {
        if (isFinished())
            throw RequestInactive();
    }

    // Skips the rest of a runInHttpThread lambda after requestComplete/onError. Unlike assertActive, passes
    // after the response was sent while the request body is still being received
    void assertNotCompleted() const REQUIRES(httpThread) ASSERT_CAPABILITY(requestActive)
    {
        if (self == nullptr)
            throw RequestInactive();
    }

    // Token owning the request. Taken by onRequest, moved to other threads with runInCpuThread and
    // runInHttpThreadWithOwner, which remove it from the caller. Debug builds check that only one thread owns it
    ThreadRole requestOwner;

#ifndef NDEBUG
    // Thread owning requestOwner. Empty while it is moved between threads or after a CPU thread dropped it
    std::atomic<std::thread::id> requestOwnerThread;
#endif

    // Takes the token, which must not be owned by any thread
    void takeRequestOwner() ASSERT_CAPABILITY(requestOwner)
    {
#ifndef NDEBUG
        auto noOwner = std::thread::id();
        const bool tookOwner = requestOwnerThread.compare_exchange_strong(noOwner, std::this_thread::get_id());
        assert(tookOwner);
#endif
    }

    void assertRequestOwner() const ASSERT_CAPABILITY(requestOwner)
    {
        assert(requestOwnerThread.load() == std::this_thread::get_id());
    }

    void releaseRequestOwner() RELEASE(requestOwner) EXCLUDES(pendingCleanup) NO_THREAD_SAFETY_ANALYSIS
    {
#ifndef NDEBUG
        const auto prevOwner = requestOwnerThread.exchange(std::thread::id());
        assert(prevOwner == std::this_thread::get_id());
#endif
    }

    // At the end of a CPU task (or body chunk, see waitForExtents) that did not move the token on
    void dropRequestOwnerIfOwned()
    {
#ifndef NDEBUG
        auto currThread = std::this_thread::get_id();
        requestOwnerThread.compare_exchange_strong(currThread, std::thread::id());
#endif
    }

    // Runs f on the global CPU executor, keeping self alive until f has run. Moves the request owner token to f, which
    // must be annotated with REQUIRES(self->requestOwner) (token is dropped after f) or RELEASE(self->requestOwner)
    // (f moves it on or releases it)
    template <typename F>
    void runInCpuThread(std::shared_ptr<S3Handler> self, F&& f) RELEASE(requestOwner)
    {
        assert(self.get() == this);
        releaseRequestOwner();
        runInCpuThreadWithoutOwner(std::move(self), [f = std::forward<F>(f)](const std::shared_ptr<S3Handler>& self) NO_THREAD_SAFETY_ANALYSIS mutable
                                   {
                                       self->takeRequestOwner();
                                       invokeWithSelf(f, self);
                                       self->dropRequestOwnerIfOwned(); });
    }

    // Runs f, which is otherwise passed to runInCpuThread, directly on the current thread. Moves the request owner token to f,
    // annotated like for runInCpuThread
    template <typename F>
    void runInCurrentThread(const std::shared_ptr<S3Handler>& self, F&& f) RELEASE(requestOwner) NO_THREAD_SAFETY_ANALYSIS
    {
        assert(self.get() == this);
        releaseRequestOwner();
        takeRequestOwner();
        invokeWithSelf(f, self);
    }

    // Runs f on the global CPU executor, keeping self alive until f has run. For tasks running concurrently to the
    // owner of the request (the body thread)
    template <typename F>
    static void runInCpuThreadWithoutOwner(std::shared_ptr<S3Handler> self, F&& f)
    {
        folly::getGlobalCPUExecutor()->add([self = std::move(self), f = std::forward<F>(f)]() mutable
                                           { invokeWithSelf(f, self); });
    }

    // Runs f, which does not use the handler, on the global CPU executor
    template <typename F>
    static void runInCpuThreadFireAndForget(F&& f)
    {
        folly::getGlobalCPUExecutor()->add(std::forward<F>(f));
    }

    // Use instead of downstream_, which must not be used after requestComplete/onError
    proxygen::ResponseHandler* downstream() const REQUIRES(httpThread, requestActive)
    {
        assert(self != nullptr);
        return downstream_;
    }

    // Calls f(self) if f takes the handler (instead of capturing its own copy), otherwise f()
    template <typename F>
    static void invokeWithSelf(F& f, const std::shared_ptr<S3Handler>& self) NO_THREAD_SAFETY_ANALYSIS
    {
        if constexpr (std::is_invocable_v<F&, const std::shared_ptr<S3Handler>&>)
            f(self);
        else
            f();
    }

    // Keeps self alive until f has run on the HTTP thread evb. f may call assertActive/assertNotCompleted
    template <typename F>
    static void runInHttpThread(folly::EventBase* evb, std::shared_ptr<S3Handler> self, F&& f)
    {
        evb->runInEventBaseThread([self = std::move(self), evb, f = std::forward<F>(f)]() NO_THREAD_SAFETY_ANALYSIS mutable
                                  {
                                      self->assertOnHttpThread(evb);
                                      try
                                      {
                                          invokeWithSelf(f, self);
                                      }
                                      catch (const RequestInactive&)
                                      {
                                      } });
    }

    // Like runInHttpThread, but moves the request owner token to f, which must be annotated with
    // REQUIRES(self->httpThread, self->requestOwner). The HTTP thread keeps the token after f
    template <typename F>
    void runInHttpThreadWithOwner(folly::EventBase* evb, std::shared_ptr<S3Handler> self, F&& f) RELEASE(requestOwner) EXCLUDES(pendingCleanup)
    {
        assert(self.get() == this);
        releaseRequestOwner();
        runInHttpThread(evb, std::move(self), [f = std::forward<F>(f)](const std::shared_ptr<S3Handler>& self) REQUIRES(self->httpThread) NO_THREAD_SAFETY_ANALYSIS mutable
                        {
                            self->takeRequestOwner();
                            invokeWithSelf(f, self); });
    }

    int64_t done_bytes GUARDED_BY(httpThread) = 0;
    // readObject paused and handed the request back to the HTTP thread. onEgressResumed resumes reading
    bool readObjectPaused GUARDED_BY(httpThread) = false;
    // Set via setFinished()
    bool finished_ GUARDED_BY(httpThread) = false;
    std::atomic<int64_t> put_remaining = -1;
    ExpatXmlParser xmlBody GUARDED_BY(httpThread);
    std::string bodyData GUARDED_BY(requestOwner);

    const std::string serverUrl;

    std::unique_ptr<SingleFileStorage::MatchInfo> matchInfo GUARDED_BY(requestOwner);
    std::unique_ptr<MultiPartUploadData> multiPartUploadData GUARDED_BY(requestOwner);
    std::unique_ptr<MultiPartDownloadData> multiPartDownloadData GUARDED_BY(requestOwner);
    std::unique_ptr<DeleteObjectsData> deleteObjectsData GUARDED_BY(requestOwner);
    std::unique_ptr<ObjMetadata> objMetadata GUARDED_BY(requestOwner);
    std::unique_ptr<PostObjectData> postObjectData GUARDED_BY(httpThread);
    std::unique_ptr<PutBucketCorsData> putBucketCorsData GUARDED_BY(requestOwner);
    std::unique_ptr<PutObjectLockConfigurationData> putObjectLockConfigurationData GUARDED_BY(requestOwner);
    std::unique_ptr<PutObjectRetentionData> putObjectRetentionData GUARDED_BY(requestOwner);

    std::mutex extents_mutex;
    std::condition_variable extents_cond;

    std::vector<SingleFileStorage::Ext> extents GUARDED_BY(extents_mutex);
    enum class ExtentsState
    {
        // CPU task preparing the write (putObject, putObjectPart, postObject) has not set extents yet
        Pending,
        Initialized,
        // Request finished (setFinished, for actions using the body thread). Overrides Initialized
        Aborted
    };
    ExtentsState extentsState GUARDED_BY(extents_mutex) = ExtentsState::Pending;

    std::vector<SingleFileStorage::Ext>& ownedExtents() EXCLUDES(extents_mutex) REQUIRES(requestOwner) NO_THREAD_SAFETY_ANALYSIS
    {
#ifndef NDEBUG
        std::scoped_lock lock(extents_mutex);
        assert(!usesBodyThread() || extentsState != ExtentsState::Pending);
#endif
        return extents;
    }
    EvpMdCtx evpMdCtx GUARDED_BY(requestOwner);
    std::string contentMd5 GUARDED_BY(requestOwner);
    std::unique_ptr<PayloadHashBase> payloadHash GUARDED_BY(requestOwner);
    std::unique_ptr<PayloadHashBase> sdkChecksumHash GUARDED_BY(requestOwner);

    struct BodyObj
    {
        int64_t offset;
        std::unique_ptr<folly::IOBuf> body;
        bool unpause;
    };

    std::mutex bodyMutex;
    bool hasBodyThread GUARDED_BY(bodyMutex) = false;
    std::queue<BodyObj> bodyQueue GUARDED_BY(bodyMutex);

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
        std::optional<std::string> trailerExpectedHash;
    };

    std::unique_ptr<AwsChunkedEncoding> awsChunkedEncoding GUARDED_BY(httpThread);

    struct ReadingMultipartObject
    {
        size_t refs = 0;
        int64_t delUploadId = -1;
        int64_t delBucketId = -1;
        std::vector<MultiPartDownloadData::PartExt> delParts;
    };

    static std::mutex readingMultipartObjectsMutex;
    static std::unordered_map<std::string, ReadingMultipartObject> readingMultipartObjects GUARDED_BY(readingMultipartObjectsMutex);

    SingleFileStorage& _sfs;

    SingleFileStorage& sfs()
    {
#ifndef NDEBUG
        sfsWasUsed = true;
#endif
        return _sfs;
    }

#ifndef NDEBUG
    enum class SigCheckResult
    {
        NotChecked,
        Ok,
        Failed
    };

    SigCheckResult sigChecked GUARDED_BY(httpThread) = SigCheckResult::NotChecked;
    std::atomic<bool> sfsWasUsed = false;
#endif

    std::string corsAllowOrigin GUARDED_BY(httpThread);
};
