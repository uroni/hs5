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

class S3Handler : public proxygen::RequestHandler
{
    SingleFileStorage &sfs;
	const std::string& root_key;

public:
    S3Handler(SingleFileStorage &sfs, const std::string& root_key) : sfs(sfs), self(this), root_key(root_key) {}

    void
    onRequest(std::unique_ptr<proxygen::HTTPMessage> headers) noexcept override;

    void onBody(std::unique_ptr<folly::IOBuf> body) noexcept override;

    void onEOM() noexcept override;

    void onUpgrade(proxygen::UpgradeProtocol proto) noexcept override;

    void requestComplete() noexcept override;

    void onError(proxygen::ProxygenError err) noexcept override;

    void onEgressPaused() noexcept override;

    void onEgressResumed() noexcept override;

private:
    void readFile(folly::EventBase *evb);
    void readObject(folly::EventBase *evb, std::shared_ptr<S3Handler> self, int64_t offset);
	void onBodyCPU(folly::EventBase *evb, int64_t offs, std::unique_ptr<folly::IOBuf> body);
    void listObjects(proxygen::HTTPMessage& headers);
    void getCommitObject(proxygen::HTTPMessage& headers);
    void getObject(proxygen::HTTPMessage& headers);
    void putObject(proxygen::HTTPMessage& headers);
    void commit(proxygen::HTTPMessage& headers);
    void deleteObject(proxygen::HTTPMessage& headers);

    void listObjects(folly::EventBase *evb, std::shared_ptr<S3Handler> self, const std::string& marker, int max_keys, const std::string& prefix, const std::string& delimiter);

	enum class RequestType
	{
		Unknown,
		GetObject,
		HeadObject,
		PutObject,
        DeleteObject,
        ListObjects
	};

	std::shared_ptr<S3Handler> self;
	RequestType request_type = RequestType::Unknown;

    std::string fpath;
    std::atomic<bool> paused_{ false };
    int64_t done_bytes = 0;
	bool running = false;
    bool finished_ = false;
	std::atomic<int64_t> put_remaining = -1;

	std::mutex extents_mutex;
	std::condition_variable extents_cond;

	std::vector<SingleFileStorage::Ext> extents;
};
