#include "StaticHandler.h"
#include "wwwgen/www_files.h"
#include <folly/FileUtil.h>
#include <folly/executors/GlobalExecutor.h>
#include <folly/io/async/EventBaseManager.h>
#include <proxygen/httpserver/RequestHandler.h>
#include <proxygen/httpserver/ResponseBuilder.h>
#include <proxygen/lib/utils/CompressionFilterUtils.h>
#include <proxygen/lib/utils/SafePathUtils.h>
#include <folly/logging/xlog.h>

using namespace proxygen;

void StaticHandler::onRequest(std::unique_ptr<HTTPMessage> headers) noexcept
{
    _error = false;
    if (headers->getMethod() != HTTPMethod::GET)
    {
        ResponseBuilder(downstream_)
            .status(400, "Bad method")
            .body("Only GET is supported")
            .sendWithEOM();
        return;
    }

    auto compressionOptions = CompressionFilterUtils::FactoryOptions();
    compressionOptions.enableZstd = false;
    const auto compression = CompressionFilterUtils::getFilterParams(*headers, compressionOptions);

    if (!compression || compression->headerEncoding != "gzip")
    {
        _decompressor = std::make_unique<ZlibStreamDecompressor>(CompressionType::GZIP);
    }

    const auto path = !headers->getPathAsStringPiece().empty() ? headers->getPathAsStringPiece().subpiece(1) : "index.html";
    const auto fpath = (path == "/" || path.empty()) ? "index.html" : path;

    const auto itFile = www_files.find(fpath);

    if (itFile == www_files.end())
    {
        XLOGF(DBG0, "File not found: {}", fpath);
        ResponseBuilder(downstream_)
            .status(404, "Not found")
            .body("File not found")
            .sendWithEOM();
        return;
    }

    const auto& etag = itFile->second.etag;

    const auto ifNoneMatch = headers->getHeaders().getSingleOrEmpty(HTTP_HEADER_IF_NONE_MATCH);

    if(ifNoneMatch == etag)
    {
        ResponseBuilder(downstream_)
            .status(304, "Not modified")
            .sendWithEOM();
        return;
    }

    _file.set(std::string_view(reinterpret_cast<const char *>(itFile->second.data), itFile->second.len));

    ResponseBuilder resp(downstream_);
    resp.status(200, "Ok")
        .header(HTTP_HEADER_ETAG, etag)
        .header(HTTP_HEADER_CONTENT_TYPE, itFile->second.contentType);

    if(itFile->second.immutable)
        resp.header(HTTP_HEADER_CACHE_CONTROL, "Cache-Control: max-age=31536000, immutable");

    if(!_decompressor)
    {
        resp.header(HTTP_HEADER_CONTENT_ENCODING, "gzip"); 
        resp.header(HTTP_HEADER_CONTENT_LENGTH, std::to_string(itFile->second.len));
    }
    else
    {
        resp.header(HTTP_HEADER_CONTENT_LENGTH, std::to_string(itFile->second.uncompLen));
    }

    resp.send();

    XLOGF(DBG0, "Sending file: {}", fpath);

    _readFileScheduled = true;

    folly::getGlobalCPUExecutor()->add(
        [this, evb = folly::EventBaseManager::get()->getEventBase()]()
        {
            readFile(evb);
        });
}

void StaticHandler::readFile(folly::EventBase *evb)
{
    folly::IOBufQueue buf;
    while (!_file.done() && !_paused)
    {
        auto data = buf.preallocate(4000, 4000);
        const auto rc = _file.read(reinterpret_cast<char *>(data.first), data.second);
        if (rc == 0)
        {
            XLOGF(DBG0, "Read EOF");
            _file.setDone();
            evb->runInEventBaseThread([this]
                                      {
                if (!_error) {
                ResponseBuilder(downstream_).sendWithEOM();
                }
            });
            break;
        }
        else
        {
            buf.postallocate(rc);
            XLOGF(DBG0, "Read {} bytes", rc);
            if (_decompressor)
            {
                auto body = buf.move();
                auto decompBuf = _decompressor->decompress(body.get());
                XLOGF(DBG0, "Decompressed {} bytes", decompBuf->length());
                evb->runInEventBaseThread([this, body = std::move(decompBuf)]() mutable
                                          {
                    if (!_error) {
                        ResponseBuilder(downstream_).body(std::move(body)).send();
                    } 
                });
            }
            else
            {
                evb->runInEventBaseThread([this, body = buf.move()]() mutable
                                          {
                if (!_error) {
                    ResponseBuilder(downstream_).body(std::move(body)).send();
                } 
                });
            }
        }
    }

    // Notify the request thread that we terminated the readFile loop
    evb->runInEventBaseThread([this]
                              {
    _readFileScheduled = false;
    if (!checkForCompletion() && !_paused) {
      VLOG(4) << "Resuming deferred readFile";
      onEgressResumed();
    } });
}

void StaticHandler::onEgressPaused() noexcept
{
    // This will terminate readFile soon
    VLOG(4) << "StaticHandler paused";
    _paused = true;
}

void StaticHandler::onEgressResumed() noexcept
{
    VLOG(4) << "StaticHandler resumed";
    _paused = false;
    // If readFileScheduled_, it will reschedule itself
    if (!_readFileScheduled && !_file.done())
    {
        _readFileScheduled = true;
        folly::getGlobalCPUExecutor()->add(
            [this, evb = folly::EventBaseManager::get()->getEventBase()]()
            {
                readFile(evb);
            });
    }
    else
    {
        VLOG(4) << "Deferred scheduling readFile";
    }
}

void StaticHandler::onBody(std::unique_ptr<folly::IOBuf> /*body*/) noexcept
{
}

void StaticHandler::onEOM() noexcept
{
}

void StaticHandler::onUpgrade(UpgradeProtocol /*protocol*/) noexcept
{
}

void StaticHandler::requestComplete() noexcept
{
    _finished = true;
    _paused = true;
    checkForCompletion();
}

void StaticHandler::onError(ProxygenError /*err*/) noexcept
{
    _error = true;
    _finished = true;
    _paused = true;
    checkForCompletion();
}

bool StaticHandler::checkForCompletion()
{
    if (_finished && !_readFileScheduled)
    {
        VLOG(4) << "deleting StaticHandler";
        _self.reset();
        return true;
    }
    return false;
}