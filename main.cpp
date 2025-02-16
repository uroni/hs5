/**
 * Copyright Martin Raiber. All Rights Reserved.
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */
#include <exception>
#include <folly/String.h>
#include <gflags/gflags.h>
#include <iostream>
#include <folly/experimental/coro/Task.h>
#include <folly/experimental/coro/BlockingWait.h>
#include <folly/init/Init.h>
#include <folly/Memory.h>
#include <folly/executors/CPUThreadPoolExecutor.h>
#include <folly/executors/GlobalExecutor.h>
#include <folly/init/Init.h>
#include <folly/io/async/EventBaseManager.h>
#include <folly/portability/GFlags.h>
#include <folly/portability/Unistd.h>
#include <folly/Random.h>
#include <folly/logging/xlog.h>
#include <proxygen/httpserver/HTTPServer.h>
#include <proxygen/httpserver/RequestHandlerFactory.h>
#include <proxygen/lib/http/session/HTTPSessionBase.h>
#include "Auth.h"
#include "s3handler.h"
#include "SingleFileStorage.h"
#include <chrono>
#include "DbDao.h"
#include "ApiHandler.h"
#include "Buckets.h"
#include "config.h"
#include "StaticHandler.h"

using namespace std::chrono_literals;

DEFINE_int32(http_port, 11000, "Port to listen on with HTTP protocol");
DEFINE_int32(h2_port, -1, "Port to listen on with HTTP/2 protocol (-1 if disabled)");
DEFINE_string(ip, "localhost", "IP/Hostname to bind to");
DEFINE_int64(data_file_size_limit_mb, 0, "Max data file size (0 for unlimited)");
DEFINE_int64(data_file_alloc_chunk_size, 512*1024*1024, "Data file chunk allocation size");
DEFINE_int32(threads,
             0,
             "Number of threads to listen on. Numbers <= 0 "
             "will use the number of cores on this machine.");
DEFINE_bool(manual_commit, false, "Manually commit by putting to a711e93e-93b4-4a9e-8a0b-688797470002");
DEFINE_string(index_path, ".", "Path where to put the index file");
DEFINE_string(data_path, ".", "Path where to put the data file");
DEFINE_bool(stop_on_error, false, "Stop on write/read errors");
DEFINE_bool(punch_holes, true, "Free up space if not enough free space is left by punching holes");
DEFINE_string(server_url, "serverurl", "URL of server");
DEFINE_bool(with_bucket_versioning, true, "Enable bucket versioning");

namespace {
  std::unique_ptr<proxygen::HTTPServer> server;

class S3HandlerFactory : public proxygen::RequestHandlerFactory {
   SingleFileStorage sfs;
 public:
  S3HandlerFactory(SingleFileStorage::SFSOptions sfsoptions)
   : sfs(std::move(sfsoptions))
  {
    sfs.start_thread(sfs.get_transid());
  }

  void onServerStart(folly::EventBase* /*evb*/) noexcept override {
  }

  void onServerStop() noexcept override {
  }

  bool isApiCall(const std::string_view path)
  {
    if(path.empty())
      return false;
    
    const auto bucketEnd = path.find_first_of('/', 1);
    if(bucketEnd == std::string::npos)
        return false;
    const auto bucketName = path.substr(1, bucketEnd);

    if(bucketName!="api-v1-b64be512-4b03-4028-a589-13931942e205/")
        return false;

    return true;
  }

  bool isStaticFile(const std::string_view path)
  {
    if(path.empty())
      return true;
    
    if(path.size() == 1 && path[0]=='/')
      return true;

    const auto bucketEnd = path.find_first_of('/', 1);
    if(bucketEnd == std::string::npos)
        return path=="/favicon.ico" || path=="/index.html";

    const auto bucketName = path.substr(1, bucketEnd);

    if(bucketName=="admin-b64be5124b034028a58913931942e205/")
        return true;

    return false;
  }

  proxygen::RequestHandler* onRequest(proxygen::RequestHandler*, proxygen::HTTPMessage* message) noexcept override
  {
    const std::string_view path(message->getPathAsStringPiece());
    
    if(message->getMethod() == proxygen::HTTPMethod::POST &&
       isApiCall(path))
      return new ApiHandler(sfs);
    if(message->getMethod() == proxygen::HTTPMethod::GET &&
        message->getHeaders().getSingleOrEmpty(proxygen::HTTP_HEADER_AUTHORIZATION).empty() &&
        isStaticFile(path))
      return new StaticHandler();

    return new S3Handler(sfs, FLAGS_server_url, FLAGS_with_bucket_versioning);
  }
};
}

void stopServer()
{
  server->stop();
}

int realMain(int argc, char* argv[])
{
    folly::Init init(&argc, &argv, true);

    XLOGF(INFO, "HS5 {} rev {}", PACKAGE_VERSION, GIT_REVISION);

    SingleFileStorage::init_mutex();
    try
    {
      DbDao::init();
      ApiHandler::init();
      refreshAuthCache();
      refreshBucketCache();
    }
    catch(const std::exception& e)
    {
      XLOGF(ERR, "Error during initialization {}: {}", typeid(e).name(), e.what());
      return 1;
    }

    if(FLAGS_server_url.empty())
      FLAGS_server_url = "http://example.com";

    std::vector<proxygen::HTTPServer::IPConfig> IPs = {
        {folly::SocketAddress(FLAGS_ip, FLAGS_http_port, true), proxygen::HTTPServer::Protocol::HTTP},
    };

    if(FLAGS_h2_port!=-1)
    {
      IPs.push_back({folly::SocketAddress(FLAGS_ip, FLAGS_h2_port, true), proxygen::HTTPServer::Protocol::HTTP2});
    }

    SingleFileStorage::SFSOptions sfsoptions;
    sfsoptions.data_path = FLAGS_data_path;
    sfsoptions.db_path = FLAGS_index_path;
    sfsoptions.data_file_size_limit_mb = FLAGS_data_file_size_limit_mb;
    sfsoptions.alloc_chunk_size = FLAGS_data_file_alloc_chunk_size;
    std::vector<unsigned char> runtime_id(32);
    folly::Random::secureRandom(runtime_id.data(), runtime_id.size());
    sfsoptions.runtime_id = folly::hexlify<std::string>(folly::ByteRange(runtime_id.data(), runtime_id.size()));
    sfsoptions.manual_commit = FLAGS_manual_commit;
    sfsoptions.stop_on_error = FLAGS_stop_on_error;
    sfsoptions.punch_holes = FLAGS_punch_holes;
    sfsoptions.key_compare_func = mdb_cmp_s3key;
    sfsoptions.common_prefix_func = s3key_common_prefix;
    sfsoptions.common_prefix_hash_func = s3key_common_prefix_hash;

    proxygen::HTTPSessionBase::setMaxReadBufferSize(16*1024);

    proxygen::HTTPServerOptions options;
    options.threads = static_cast<size_t>(FLAGS_threads);
    options.idleTimeout = 60s;
    options.shutdownOn = {SIGINT, SIGTERM};
    options.enableContentCompression = false;
    options.handlerFactories =
        proxygen::RequestHandlerChain().addThen<S3HandlerFactory>(sfsoptions).build();
    options.h2cEnabled = true;

    server = std::make_unique<proxygen::HTTPServer>(std::move(options));
    server->bind(IPs);

    for(const auto& ip: IPs)
    {
      XLOGF(INFO, "Listening on {}", ip.address.describe());
    }

    XLOGF(INFO, "Config: Bucket versioning: {}, Punch holes: {}, Stop on error: {}, Manual commit: {}", FLAGS_with_bucket_versioning, FLAGS_punch_holes, FLAGS_stop_on_error, FLAGS_manual_commit);

    std::thread t([&]() {
      server->start();
      server.reset();
      });
    t.join();
    return 0;
}


