#pragma once

#include <string>
#include <stdexcept>

#include "DbDao.h"
#include "apigen/AddUserParams.hpp"
#include "apigen/AddUserResp.hpp"
#include "apigen/Herror.hpp"
#include "apigen/LoginParams.hpp"
#include "apigen/LoginResp.hpp"
#include "apigen/HapiError.hpp"
#include "apigen/ListResp.hpp"
#include "apigen/ListParams.hpp"
#include "apigen/AddBucketResp.hpp"
#include "apigen/AddBucketParams.hpp"
#include "apigen/StatsParams.hpp"
#include "apigen/StatsResp.hpp"
#include "Session.h"
#include <proxygen/httpserver/RequestHandler.h>


class SingleFileStorage;

class ApiHandler : public proxygen::RequestHandler
{
public:
    ApiHandler(SingleFileStorage &sfs);

    void onRequest(std::unique_ptr<proxygen::HTTPMessage> headers) noexcept override;

    bool onBody(const uint8_t* data, size_t dataSize);

    void onBody(std::unique_ptr<folly::IOBuf> body) noexcept override;

    void onEOM() noexcept override;

    void onUpgrade(proxygen::UpgradeProtocol proto) noexcept override;

    void requestComplete() noexcept override;

    void onError(proxygen::ProxygenError err) noexcept override;

    struct ApiResponse
    {
        int code;
        std::string body;
        std::optional<std::string> setCookie;
        std::optional<std::string> contentType;
    };

    ApiResponse runRequest();

    static void init();

private:
    Api::AddUserResp addUser(const Api::AddUserParams& params, const ApiSessionStorage& sessionStorage);
    std::pair<Api::LoginResp, std::string> login(const Api::LoginParams& params);
    Api::ListResp list(const Api::ListParams& params, const ApiSessionStorage& sessionStorage);
    Api::ListResp listBuckets(const Api::ListParams& params, const ApiSessionStorage& sessionStorage);
    Api::AddBucketResp addBucket(const Api::AddBucketParams& params, const ApiSessionStorage& sessionStorage);
    Api::StatsResp stats(const Api::StatsParams& params, const ApiSessionStorage& sessionStorage);

    std::string func;
    std::string body;
    std::string cookieSes;

    SingleFileStorage &sfs;

    static thread_local DbDao dao;

    int64_t putRemaining = -1;
    int64_t doneBytes = 0;
    bool finished = false;

    std::unique_ptr<ApiHandler> self;
};

class ApiError : std::runtime_error
{
    Api::Herror error;
public:
    ApiError(Api::Herror error, std::string msg)
        :error(error), std::runtime_error(std::move(msg)) {}

    ApiError(Api::Herror error) 
        : error(error), std::runtime_error("ApiError "+std::to_string(static_cast<int>(error)))
        {}

    Api::HapiError response() const
    {
        Api::HapiError ret;
        ret.herror = error;
        ret.msg = what();
        return ret;
    }
};
