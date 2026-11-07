#pragma once
#include "ThreadSafety.h"

#include <atomic>
#include <cassert>
#include <folly/File.h>
#include <folly/Memory.h>
#include <folly/io/async/EventBase.h>
#include <folly/io/async/EventBaseManager.h>
#include <proxygen/httpserver/RequestHandler.h>
#include <proxygen/lib/utils/ZlibStreamDecompressor.h>
#include <string>

namespace proxygen
{
    class ZlibStreamDecompressor;
}

class MemFile
{
public:
    MemFile(const std::string_view data)
        : _data(data)
    {
    }

    void set(const std::string_view data)
    {
        _data = data;
    }

    int read(char* buf, size_t bufSize)
    {
        if (_data.empty())
            return 0;

        const auto toRead = std::min(bufSize, _data.size());
        memcpy(buf, _data.data(), toRead);
        _data.remove_prefix(toRead);
        return toRead;
    }

    bool done() const
    {
        return _done;
    }

    void setDone()
    {
        _done = true;
    }

private:
    std::string_view _data;
    bool _done{ false };
};

class StaticHandler : public proxygen::RequestHandler
{
public:
    StaticHandler() : _self(this) {}

    void onRequest(std::unique_ptr<proxygen::HTTPMessage> headers) noexcept override;

    void onBody(std::unique_ptr<folly::IOBuf> body) noexcept override;

    void onEOM() noexcept override;

    void onUpgrade(proxygen::UpgradeProtocol proto) noexcept override;

    void requestComplete() noexcept override;

    void onError(proxygen::ProxygenError err) noexcept override;

    void onEgressPaused() noexcept override;

    void onEgressResumed() noexcept override;

private:
    void readFile(folly::EventBase* evb) REQUIRES(requestOwner);
    bool checkForCompletion() REQUIRES(httpThread);

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

    // Role held by the owner of _file and _decompressor: readFile while it is scheduled, the HTTP thread otherwise
    ThreadRole requestOwner;

    void assertHttpThreadOwnsRequest() const REQUIRES(httpThread) ASSERT_CAPABILITY(requestOwner)
    {
        assert(!_readFileScheduled);
    }

    // _readFileScheduled is set before scheduling readFile and only reset by the lambda readFile posts last
    void assertReadFileOwnsRequest() const ASSERT_CAPABILITY(requestOwner) NO_THREAD_SAFETY_ANALYSIS
    {
        assert(_readFileScheduled);
    }

    bool fileDone() REQUIRES(httpThread)
    {
        assertHttpThreadOwnsRequest();
        return _file.done();
    }

    MemFile _file GUARDED_BY(requestOwner){ std::string_view() };
    bool _readFileScheduled GUARDED_BY(httpThread){ false };
    std::atomic<bool> _paused{ false };
    bool _finished GUARDED_BY(httpThread){ false };
    std::atomic<bool> _error{ false };
    std::unique_ptr<StaticHandler> _self GUARDED_BY(httpThread);
    std::unique_ptr<proxygen::ZlibStreamDecompressor> _decompressor GUARDED_BY(requestOwner);
};
