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
#include "Session.h"

class S3Handler;

class FunctionNotFoundError : std::runtime_error
{
public:
    FunctionNotFoundError(const std::string& msg)
        : std::runtime_error(msg) {}
}; 

class ApiHandler
{
public:
    ApiHandler(const std::string_view func, const std::string_view cookieSes, S3Handler& s3handler);

    bool onBody(const uint8_t* data, size_t dataSize);

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

    std::string func;
    std::string body;
    std::string cookieSes;

    S3Handler& s3handler;

    static thread_local DbDao dao;
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
