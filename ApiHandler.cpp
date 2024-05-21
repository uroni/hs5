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
#include "apigen/HapiError.hpp"
#include "apigen/Herror.hpp"
#include "apigen/GeneratorsAddUserParams.hpp"
#include "apigen/GeneratorsAddUserResp.hpp"
#include "apigen/GeneratorsLoginResp.hpp"
#include "apigen/GeneratorsLoginParams.hpp"
#include "apigen/GeneratorsHapiError.hpp"
#include "apigen/GeneratorsListResp.hpp"
#include "apigen/GeneratorsListParams.hpp"
#include <argon2.h>
#include <folly/Random.h>
#include <iostream>
#include <gflags/gflags.h>
#include <folly/logging/xlog.h>
#include "Buckets.h"
#include "apigen/Object.hpp"
#include "s3handler.h"
#include "SingleFileStorage.h"
#include "folly/ScopeGuard.h"

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
            throw ApiError(Api::Herror::argonEncoding, "Error encoding password via argon2i");

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

ApiHandler::ApiHandler(const std::string_view func, const std::string_view cookieSes, S3Handler& s3handler)
 : func(func), cookieSes(cookieSes), s3handler(s3handler)
{
    if(func!="adduser"
        && func!="login"
        && func!="list")
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
                throw ApiError(Api::Herror::sessionRequired);

            auto session = getSession(params["ses"].get<std::string>(), cookieSes);
            if(!session)
                throw ApiError(Api::Herror::sessionNotFound);
            
            if(func=="adduser")
                resp = addUser(params, *session);
            else if(func=="list")
                resp = list(params, *session);
        }
        
    }
    catch(const json::exception& e)
    {
        return ApiResponse{.code=500, .body=fmt::format("JSON exception id {}: {}", e.id, e.what())};
    }
    catch(const ApiError& e)
    {
        json resp = e.response();
        if( e.response().herror == Api::Herror::sessionNotFound)
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
        throw ApiError(Api::Herror::argonEncoding, "Error encoding password via argon2i");

    if(dao.getUserByName(params.username).exists)
        throw ApiError(Api::Herror::userAlreadyExists, fmt::format("User {} already exists", params.username));

    dao.addUser(params.username, 0, encoded, 0);

    return {};
}

std::pair<Api::LoginResp, std::string> ApiHandler::login(const Api::LoginParams& params)
{
    auto user = dao.getUserByName(params.username);
    if(!user.exists)
        throw ApiError(Api::Herror::userNotFound, fmt::format("User {} not found", params.username));

    if(user.password_state!=0)
        throw ApiError(Api::Herror::unknownPasswordHashing);

    const auto rc = argon2i_verify(user.password.c_str(), params.password.data(), params.password.size());
    if(rc != ARGON2_OK)
        throw ApiError(Api::Herror::passwordWrong, fmt::format("Supplied password for user {} wrong", params.username));

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

Api::ListResp ApiHandler::list(const Api::ListParams& params, const ApiSessionStorage& sessionStorage)
{
    if(params.path.empty() || params.path[0]!='/')
        throw ApiError(Api::Herror::invalidPath);
    if(params.path=="/")
        return listBuckets(params, sessionStorage);

    const auto& path = params.path;
    const size_t bucketNameEnd = path.find('/', 1);

    const auto bucketName = bucketNameEnd==std::string::npos ? path : path.substr(1, bucketNameEnd);
    const auto prefix = bucketNameEnd==std::string::npos ? std::string() : path.substr(bucketNameEnd+1);

    if(!prefix.empty() && prefix[prefix.size()-1]!='/')
        throw ApiError(Api::Herror::invalidPath);

    const auto bucketIdOpt = getBucket(bucketName);
    if(!bucketIdOpt)
        throw ApiError(Api::Herror::bucketNotFound);
    const auto bucketId = *bucketIdOpt;
    auto& sfs = s3handler.sfs;
    const auto iterStartVal = make_key({.key = params.continuationToken ? *params.continuationToken : prefix, .version=std::numeric_limits<int64_t>::max(), .bucketId = bucketId});

    SingleFileStorage::IterData iterData = {};
    if(!sfs.iter_start(iterStartVal, false, iterData))
        throw ApiError(Api::Herror::errorStartingListing);

    auto guard = folly::makeGuard([&] { sfs.iter_stop(iterData); });

    Api::ListResp resp;
    resp.isTruncated = true;
    std::string lastOutputKeyStr;

    while(resp.objects.size()<1000)
    {
        std::string keyBin, md5sum;
        int64_t offset, size, last_modified;
        std::vector<SingleFileStorage::SPunchItem> extra_exts;
        if(!sfs.iter_curr_val(keyBin, offset, size, extra_exts, last_modified, md5sum, iterData))
        {
            resp.isTruncated = false;
            break;
        }

        const auto keyInfo = extractKeyInfoView(keyBin);

        if(!keyInfo.key.starts_with(prefix))
        {
            resp.isTruncated = false;
            break;
        }

        if(keyInfo.bucketId != bucketId)
        {
            resp.isTruncated = false;
            break;
        }

        bool outputKey = true;

        if(keyInfo.key == lastOutputKeyStr)
        {
            outputKey = false;
        }
        else if(md5sum.size() == 1 && md5sum[0] == metadata_tombstone)
        {
            outputKey = false;
        }

        const size_t delimPos = keyInfo.key.find_first_of('/', prefix.size());
        if(outputKey && delimPos != std::string::npos)
        {
            Api::Object obj;
            obj.name = std::string(keyInfo.key.substr(prefix.size(), delimPos - prefix.size() + 1));
            obj.size = 0;
            obj.type = 2;
            if(obj.name!=lastOutputKeyStr)
                resp.objects.emplace_back(std::move(obj));
            outputKey = false;
            lastOutputKeyStr = obj.name;
        }

        if (outputKey)
        {
            lastOutputKeyStr = keyInfo.key;
            
            for(const auto& ext: extra_exts)
            {
                size += ext.len;
            }

            Api::Object obj;
            obj.name = std::string(keyInfo.key.substr(prefix.size()));
            obj.size = size;
            obj.type = 3;
            resp.objects.emplace_back(std::move(obj));
        }

        if(!sfs.iter_next(iterData))
            throw ApiError(Api::Herror::errorListingInIteration);
    }

    if(resp.isTruncated)
    {
        std::string data;
        std::string keyBin;
        sfs.iter_curr_val(keyBin, data, iterData);

        const auto keyInfo = extractKeyInfoView(keyBin);
        resp.nextMarker = keyInfo.key;
    }

    return resp;
}

Api::ListResp ApiHandler::listBuckets(const Api::ListParams& params, const ApiSessionStorage& sessionStorage)
{
    if(params.continuationToken)
        throw ApiError(Api::Herror::unexpectedContinuationToken);

    return getBucketNames();
}