#pragma once

#include <string>
#include <stdexcept>

#include "DbDao.h"
#include "apigen/AddUserParams.hpp"
#include "apigen/AddUserResp.hpp"
#include "apigen/LoginParams.hpp"
#include "apigen/LoginResp.hpp"
#include "apigen/ApiError.hpp"
#include "Session.h"

class FunctionNotFoundError : std::runtime_error
{
public:
    FunctionNotFoundError(const std::string& msg)
        : std::runtime_error(msg) {}
}; 

class ApiHandler
{
public:
    ApiHandler(const std::string_view func, const std::string_view cookieSes);

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

    std::string func;
    std::string body;
    std::string cookieSes;

    static thread_local DbDao dao;
};

class ApiError : std::runtime_error
{
    Api::Error error;
public:
    ApiError(Api::Error error, std::string msg)
        :error(error), std::runtime_error(std::move(msg)) {}

    ApiError(Api::Error error) 
        : error(error), std::runtime_error("ApiError "+std::to_string(static_cast<int>(error)))
        {}

    Api::ApiError response() const
    {
        Api::ApiError ret;
        ret.error = error;
        ret.msg = what();
        return ret;
    }
};
