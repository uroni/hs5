/**
 * Copyright Martin Raiber. All Rights Reserved.
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */
#include "ApiHandler.h"
#include <fmt/core.h>
#include <folly/Range.h>
#include <folly/String.h>
#include <nlohmann/json.hpp>
#include "DbDao.h"
#include "apigen/Error.hpp"
#include "apigen/GeneratorsAddUserParams.hpp"
#include "apigen/GeneratorsAddUserResp.hpp"
#include "apigen/GeneratorsLoginResp.hpp"
#include "apigen/GeneratorsLoginParams.hpp"
#include "apigen/GeneratorsApiError.hpp"
#include <argon2.h>
#include <folly/Random.h>
#include <iostream>
#include <gflags/gflags.h>
#include <folly/logging/xlog.h>

DEFINE_string(init_root_password, "", "Initial password of root account");
DEFINE_string(init_root_access_key, "", "Initial name of access key of root account. Default: root");
DEFINE_string(init_root_secret_access_key, "", "Initial secret access key of root account. Default: Password of root account");

using json = nlohmann::json;

thread_local DbDao ApiHandler::dao;

void ApiHandler::init()
{
    DbDao dao;
    if(!dao.hasUser())
    {
        sqlgen::ScopedManualCommitWriteTransaction trans(&dao.getDb());

        std::string password;
        const auto envPassword = getenv("INIT_ROOT_PASSWORD");
        bool printSecretAccessKey = false;
        if(FLAGS_init_root_password.empty() && envPassword==nullptr)
        {
            char pw[16];
            folly::Random::secureRandom(pw, sizeof(pw));

            password = folly::hexlify(folly::StringPiece(pw, sizeof(pw)));

            std::cout << fmt::format("No user found in db. Adding root user with pw: {}", password) << std::endl;
            XLOGF(INFO, "No user found in db. Adding root user with generated pw");
            printSecretAccessKey = true;
        }
        else if(!FLAGS_init_root_password.empty())
        {
            password = FLAGS_init_root_password;
            XLOGF(INFO, "No user found in db. Adding root user with password in parameters");
        }
        else
        {
            password = envPassword;
            XLOGF(INFO, "No user found in db. Adding root user with password in environment variable");
        }

        char salt[16];
        folly::Random::secureRandom(salt, sizeof(salt));
        char encoded[200];

        const auto rc = argon2i_hash_encoded(4, 1<<16, 2, password.data(), password.size(), salt, sizeof(salt), 32, encoded, sizeof(encoded)); 
        if(rc!=ARGON2_OK)
            throw ApiError(Api::Error::argonEncoding, "Error encoding password via argon2i");

        const auto rootUserId = dao.addUser("root", 0, encoded, 1);

        std::string initRootAccessKey = "root";
        if(!FLAGS_init_root_access_key.empty())
            initRootAccessKey = FLAGS_init_root_access_key;
        else if(const auto envAccessKey = getenv("INIT_ROOT_ACCESS_KEY"); envAccessKey!=nullptr)
            initRootAccessKey = envAccessKey;

        std::string initSecretAccessKey = password;
        if(!FLAGS_init_root_secret_access_key.empty())
            initSecretAccessKey = FLAGS_init_root_secret_access_key;
        else if(const auto envSecretAccessKey = getenv("INIT_ROOT_SECRET_ACCESS_KEY"); envSecretAccessKey!=nullptr)
            initSecretAccessKey = envSecretAccessKey;

        const auto rootRoleId = dao.addRole("root", 1);

        const auto rootPolicyId = dao.addPolicy("root", "System root policy (can do everything)", 0, R"""(
            {
                "Version": "2012-10-17",
                "Statement": [
                    {
                    "Sid": "AllowAll",
                    "Effect": "Allow",
                    "Action": "*",
                    "Resource": "*"
                    }
                 ]
            }
        )""", 1);

        dao.addRolePolicy(rootRoleId, rootPolicyId, 1);
        dao.addUserRole(rootUserId, rootRoleId, 1);

        XLOGF(INFO, "Adding access key {} secret {}", initRootAccessKey, initSecretAccessKey);

        dao.addAccessKey(rootUserId, "Root access key (can access everything)", initRootAccessKey, initSecretAccessKey, 1);

        trans.commit();

        if(printSecretAccessKey)
            std::cout << fmt::format("Added root account accesskey: name={} secret={}", initRootAccessKey, initSecretAccessKey) << std::endl;
    }
    else
    {
        XLOGF(DBG, "Found user in db. Not initializing.");
    }
}

ApiHandler::ApiHandler(const std::string_view func, const std::string_view cookieSes)
 : func(func), cookieSes(cookieSes)
{
    if(func!="adduser"
        && func!="login")
        throw FunctionNotFoundError(fmt::format("Api function \"{}\" not found", func));
}

bool ApiHandler::onBody(const uint8_t* data, size_t dataSize)
{
    body.append(reinterpret_cast<const char*>(data), dataSize);
    return true;
}

ApiHandler::ApiResponse ApiHandler::runRequest()
{
    std::optional<json> resp;
    std::optional<std::string> setCookie;
    try
    {
        if(func=="login")
        {
            auto rc = login(json::parse(body));
            resp = rc.first;
            setCookie = std::move(rc.second);
        }
        else
        {
            auto params = json::parse(body);

            if(!params.contains("ses") || !params["ses"].is_string())
                throw ApiError(Api::Error::sessionRequired);

            auto session = getSession(params["ses"].get<std::string>(), cookieSes);
            if(!session)
                throw ApiError(Api::Error::sessionNotFound);
            
            if(func=="adduser")
                resp = addUser(params, *session);
        }
        
    }
    catch(const json::exception& e)
    {
        return ApiResponse{.code=500, .body=fmt::format("JSON exception id {}: {}", e.id, e.what())};
    }
    catch(const ApiError& e)
    {
        json resp = e.response();
        if( e.response().error == Api::Error::sessionNotFound)
            return ApiResponse{.code=401, .body=resp.dump(), .contentType="application/json"};

        return ApiResponse{.code=400, .body=resp.dump(), .contentType="application/json"};
    }
    catch(const std::exception& e)
    {
        return ApiResponse{.code=500, .body=fmt::format("Exception: {}", e.what())};
    }

    if(!resp)
        return ApiResponse{.code=500, .body="Not implemented"};

    return ApiResponse{.code=200, .body=resp->dump(), .setCookie=setCookie, .contentType="application/json"};
}

Api::AddUserResp ApiHandler::addUser(const Api::AddUserParams& params, const ApiSessionStorage& sessionStorage)
{
    char salt[16];
    folly::Random::secureRandom(salt, sizeof(salt));
    char encoded[200];

    const auto rc = argon2i_hash_encoded(4, 1<<16, 2, params.password.data(), params.password.size(), salt, sizeof(salt), 32, encoded, sizeof(encoded)); 
    if(rc!=ARGON2_OK)
        throw ApiError(Api::Error::argonEncoding, "Error encoding password via argon2i");

    if(dao.getUserByName(params.username).exists)
        throw ApiError(Api::Error::userAlreadyExists, fmt::format("User {} already exists", params.username));

    dao.addUser(params.username, 0, encoded, 0);

    return {};
}

std::pair<Api::LoginResp, std::string> ApiHandler::login(const Api::LoginParams& params)
{
    auto user = dao.getUserByName(params.username);
    if(!user.exists)
        throw ApiError(Api::Error::userNotFound, fmt::format("User {} not found", params.username));

    if(user.password_state!=0)
        throw ApiError(Api::Error::unknownPasswordHashing);

    const auto rc = argon2i_verify(user.password.c_str(), params.password.data(), params.password.size());
    if(rc != ARGON2_OK)
        throw ApiError(Api::Error::passwordWrong, fmt::format("Supplied password for user {} wrong", params.username));

    char jsSes[16];
    folly::Random::secureRandom(jsSes, sizeof(jsSes));
    char cookieSes[16];
    folly::Random::secureRandom(cookieSes, sizeof(cookieSes));

    Api::LoginResp resp;
    resp.ses = folly::hexlify(folly::StringPiece(jsSes, sizeof(jsSes)));

    auto cookieSesHex = folly::hexlify(folly::StringPiece(cookieSes, sizeof(cookieSes)));

    ApiSessionStorage apiSessionStorage{.userId = user.id};
    newSession(resp.ses, cookieSesHex, std::move(apiSessionStorage));

    return std::make_pair(resp, cookieSesHex);
}
