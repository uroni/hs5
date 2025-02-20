#pragma once

#include <atomic>
#include <string>
#include <folly/File.h>
#include <folly/Memory.h>
#include <proxygen/httpserver/RequestHandler.h>
#include <proxygen/lib/utils/ZlibStreamDecompressor.h>

namespace proxygen
{
    class ZlibStreamDecompressor;
}

class MemFile 
{
public:
    MemFile(const std::string_view data)
        : _data(data)
    {}

    void set(const std::string_view data)
    {
        _data = data;
    }

    int read(char* buf, size_t bufSize)
    {
        if(_data.empty())
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
    bool _done{false};
};


class StaticHandler : public proxygen::RequestHandler {
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
  void readFile(folly::EventBase* evb);
  bool checkForCompletion();

  MemFile _file{std::string_view()};
  bool _readFileScheduled{false};
  std::atomic<bool> _paused{false};
  bool _finished{false};
  std::atomic<bool> _error{false};
  std::unique_ptr<StaticHandler> _self;
  std::unique_ptr<proxygen::ZlibStreamDecompressor> _decompressor;
};
