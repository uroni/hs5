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
#include "apigen/GeneratorsRemoveUserParams.hpp"
#include "apigen/GeneratorsRemoveUserResp.hpp"
#include "apigen/GeneratorsListUsersParams.hpp"
#include "apigen/GeneratorsListUsersResp.hpp"
#include "apigen/GeneratorsAddAccessKeyParams.hpp"
#include "apigen/GeneratorsAddAccessKeyResp.hpp"
#include "apigen/GeneratorsListAccessKeysParams.hpp"
#include "apigen/GeneratorsListAccessKeysResp.hpp"
#include "apigen/GeneratorsRemoveAccessKeyParams.hpp"
#include "apigen/GeneratorsRemoveAccessKeyResp.hpp"
#include "apigen/GeneratorsAddPolicyParams.hpp"
#include "apigen/GeneratorsAddPolicyResp.hpp"
#include "apigen/GeneratorsRemovePolicyParams.hpp"
#include "apigen/GeneratorsRemovePolicyResp.hpp"
#include "apigen/GeneratorsListPoliciesParams.hpp"
#include "apigen/GeneratorsListPoliciesResp.hpp"
#include "apigen/GeneratorsAddRoleParams.hpp"
#include "apigen/GeneratorsAddRoleResp.hpp"
#include "apigen/GeneratorsRemoveRoleParams.hpp"
#include "apigen/GeneratorsRemoveRoleResp.hpp"
#include "apigen/GeneratorsListRolesParams.hpp"
#include "apigen/GeneratorsListRolesResp.hpp"
#include "apigen/GeneratorsAddUserRoleParams.hpp"
#include "apigen/GeneratorsAddUserRoleResp.hpp"
#include "apigen/GeneratorsRemoveUserRoleParams.hpp"
#include "apigen/GeneratorsRemoveUserRoleResp.hpp"
#include "apigen/GeneratorsListUserRolesParams.hpp"
#include "apigen/GeneratorsListUserRolesResp.hpp"
#include "apigen/GeneratorsAddRolePolicyParams.hpp"
#include "apigen/GeneratorsAddRolePolicyResp.hpp"
#include "apigen/GeneratorsRemoveRolePolicyParams.hpp"
#include "apigen/GeneratorsRemoveRolePolicyResp.hpp"
#include "apigen/GeneratorsListRolePoliciesParams.hpp"
#include "apigen/GeneratorsListRolePoliciesResp.hpp"
#include "apigen/GeneratorsChangePolicyParams.hpp"
#include "apigen/GeneratorsChangePolicyResp.hpp"
#include "apigen/GeneratorsLoginResp.hpp"
#include "apigen/GeneratorsLoginParams.hpp"
#include "apigen/GeneratorsHapiError.hpp"
#include "apigen/GeneratorsListResp.hpp"
#include "apigen/GeneratorsListParams.hpp"
#include "apigen/GeneratorsSessionCheckParams.hpp"
#include "apigen/GeneratorsSessionCheckResp.hpp"
#include "apigen/GeneratorsAddBucketResp.hpp"
#include "apigen/GeneratorsAddBucketParams.hpp"
#include "apigen/GeneratorsStatsParams.hpp"
#include "apigen/GeneratorsStatsResp.hpp"
#include "apigen/GeneratorsDeleteBucketParams.hpp"
#include "apigen/GeneratorsDeleteBucketResp.hpp"
#include "apigen/GeneratorsChangePasswordParams.hpp"
#include "apigen/GeneratorsChangePasswordResp.hpp"
#include "apigen/GeneratorsLogoutParams.hpp"
#include "apigen/GeneratorsLogoutResp.hpp"
#include "apigen/GeneratorsListBucketPermissionsParams.hpp"
#include "apigen/GeneratorsListBucketPermissionsResp.hpp"
#include "apigen/GeneratorsAddBucketPermissionParams.hpp"
#include "apigen/GeneratorsAddBucketPermissionResp.hpp"
#include "apigen/GeneratorsRemoveBucketPermissionParams.hpp"
#include "apigen/GeneratorsRemoveBucketPermissionResp.hpp"
#include "apigen/GeneratorsSetBucketPublicParams.hpp"
#include "apigen/GeneratorsSetBucketPublicResp.hpp"
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
#include <proxygen/httpserver/ResponseBuilder.h>
#include "utils.h"
#include "Buckets.h"
#include "Auth.h"
#include <map>
#include <frozen/unordered_map.h>
#include <frozen/string.h>

using namespace proxygen;

DEFINE_string(init_root_password, "", "Initial password of root account");
DEFINE_string(init_root_access_key, "", "Initial name of access key of root account. Default: root");
DEFINE_string(init_root_secret_access_key, "", "Initial secret access key of root account. Default: Password of root account");
DEFINE_string(init_create_bucket, "", "Comma separated list of buckets to create on initialization. Example: \"bucket1,bucket2\"");
DECLARE_bool(pre_sync_commit);

using json = nlohmann::json;

thread_local DbDao ApiHandler::dao;

namespace
{
    std::string getEncodedPassword(const std::string& password)
    {
        char salt[16];
        folly::Random::secureRandom(salt, sizeof(salt));
        char encoded[200];

        const auto rc = argon2i_hash_encoded(4, 1<<16, 2, password.data(), password.size(), salt, sizeof(salt), 32, encoded, sizeof(encoded)); 
        if(rc!=ARGON2_OK)
            throw ApiError(Api::Herror::argonEncoding, "Error encoding password via argon2i");

        return std::string(encoded);
    }

    void verifyPassword(const DbDao::User& user, const std::string& password)
    {
        if(user.password_state!=0)
            throw ApiError(Api::Herror::unknownPasswordHashing);

        const auto rc = argon2i_verify(user.password.c_str(), password.data(), password.size());
        if(rc != ARGON2_OK)
            throw ApiError(Api::Herror::passwordWrong, fmt::format("Supplied password for user {} wrong", user.name));
    }
}

class ApiHandler::ApiFunctions
{
public:
using api_func_t = nlohmann::json(*)(ApiHandler& apiHandler, const nlohmann::json&, const ApiSessionStorage&);

#define ADD_API_FUNC(x) {#x, [](ApiHandler& apiHandler, const nlohmann::json& params, const ApiSessionStorage& sessionStorage) -> nlohmann::json { return std::invoke(&ApiHandler::x, apiHandler, params, sessionStorage); }}

    static constexpr frozen::unordered_map<frozen::string, api_func_t, 29> apiFunctions = {
        ADD_API_FUNC(addUser),
        ADD_API_FUNC(removeUser),
        ADD_API_FUNC(listUsers),
        ADD_API_FUNC(addAccessKey),
        ADD_API_FUNC(listAccessKeys),
        ADD_API_FUNC(removeAccessKey),
        ADD_API_FUNC(addPolicy),
        ADD_API_FUNC(removePolicy),
        ADD_API_FUNC(listPolicies),
        ADD_API_FUNC(addRole),
        ADD_API_FUNC(removeRole),
        ADD_API_FUNC(listRoles),
        ADD_API_FUNC(addUserRole),
        ADD_API_FUNC(removeUserRole),
        ADD_API_FUNC(listUserRoles),
        ADD_API_FUNC(addRolePolicy),
        ADD_API_FUNC(removeRolePolicy),
        ADD_API_FUNC(listRolePolicies),
        ADD_API_FUNC(changePolicy),
        ADD_API_FUNC(changePassword),
        ADD_API_FUNC(list),
        ADD_API_FUNC(sessionCheck),
        ADD_API_FUNC(addBucket),
        ADD_API_FUNC(stats),
        ADD_API_FUNC(deleteBucket),
        ADD_API_FUNC(listBucketPermissions),
        ADD_API_FUNC(addBucketPermission),
        ADD_API_FUNC(removeBucketPermission),
        ADD_API_FUNC(setBucketPublic)
    };

#undef ADD_API_FUNC
};

void ApiHandler::init()
{
    if(!dao.hasUser())
    {
        sqlgen::ScopedManualCommitWriteTransaction trans(&dao.getDb());

        std::string password;
        const auto envPassword = getenv("HS5_INIT_ROOT_PASSWORD");
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

        const auto encoded = getEncodedPassword(password);

        const auto rootUserId = dao.addUser("root", 0, encoded, 1);

        std::string initRootAccessKey = "root";
        if(!FLAGS_init_root_access_key.empty())
            initRootAccessKey = FLAGS_init_root_access_key;
        else if(const auto envAccessKey = getenv("HS5_INIT_ROOT_ACCESS_KEY"); envAccessKey!=nullptr)
            initRootAccessKey = envAccessKey;

        std::string initSecretAccessKey = password;
        if(!FLAGS_init_root_secret_access_key.empty())
            initSecretAccessKey = FLAGS_init_root_secret_access_key;
        else if(const auto envSecretAccessKey = getenv("HS5_INIT_ROOT_SECRET_ACCESS_KEY"); envSecretAccessKey!=nullptr)
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

        const auto statementId = dao.addPolicyStatement(rootPolicyId.value(), "AllowAll", static_cast<int>(Statement::Effect::Allow));

        dao.addPolicyStatementAction(statementId, static_cast<int>(Action::AllActions));
        dao.addPolicyStatementResource(statementId, "*");

        dao.addRolePolicy(rootRoleId.value(), rootPolicyId.value(), 1);
        dao.addUserRole(rootUserId.value(), rootRoleId.value(), 1);
        
        dao.addAccessKey(rootUserId.value(), "Root access key (can access everything)", initRootAccessKey, initSecretAccessKey, 1);

        auto initialBuckets = FLAGS_init_create_bucket;
        if(const auto initialBucketsEnv = getenv("HS5_INIT_CREATE_BUCKET"); initialBucketsEnv!=nullptr)
            initialBuckets = initialBucketsEnv;

        if(!initialBuckets.empty())
        {
            XLOGF(INFO, "Creating initial buckets from parameters");
            std::vector<std::string> bucketsToCreate;
            folly::split(',', initialBuckets, bucketsToCreate, true);
            for(const auto& bucketName : bucketsToCreate)
            {
                if(bucketName.empty())
                    continue;

                if(!buckets::isValidBucketName(bucketName))
                {
                    XLOGF(WARNING, "Invalid bucket name {}. Skipping creation of this bucket.", bucketName);
                    continue;
                }

                buckets::addBucket(bucketName, false);
                XLOGF(INFO, "Created initial bucket {}", bucketName);
            }
        }

        trans.commit();

        if(printSecretAccessKey)
            std::cout << fmt::format("Added initial root account. Username: {} Password: {}", initRootAccessKey, initSecretAccessKey) << std::endl;
    }
    else
    {
        XLOGF(DBG, "Found user in db. Not initializing.");
    }
}

ApiHandler::ApiHandler(SingleFileStorage &sfs)
 : sfs(sfs), self(this)
{
}

void ApiHandler::onRequest(std::unique_ptr<proxygen::HTTPMessage> headers) noexcept
{
    if (headers->getMethod() != HTTPMethod::POST)
    {
        ResponseBuilder(downstream_)
            .status(400, "Bad method")
            .body("Only POST is supported")
            .sendWithEOM();
        return;
    }

    const std::string_view path(headers->getPathAsStringPiece());
    if(path.empty())
        return;

    const auto bucketEnd = path.find_first_of('/', 1);
    if(bucketEnd == std::string::npos)
        return;

    func = path.substr(bucketEnd+1);

    authHeader = headers->getHeaders().getSingleOrEmpty(HTTP_HEADER_AUTHORIZATION);

    if(authHeader.starts_with("Bearer "))
        authHeader = authHeader.substr(7);

    std::string cl = headers->getHeaders().getSingleOrEmpty(
                    proxygen::HTTP_HEADER_CONTENT_LENGTH);
    if (cl.empty())
    {
        ResponseBuilder(downstream_)
            .status(500, "Internal error")
            .body("Content-Length header not set")
            .sendWithEOM();
        finished = true;
        return;
    }

    if(func!="login"
        && func!="logout"
        && ApiFunctions::apiFunctions.find(frozen::string(func))==ApiFunctions::apiFunctions.end())
    {
        ResponseBuilder(downstream_)
            .status(404, "Not found")
            .body("Function not found")
            .sendWithEOM();
        finished = true;
        return;
    }

    putRemaining = std::atoll(cl.c_str());

    cookieSes = headers->getCookie("ses");
}

void ApiHandler::onBody(std::unique_ptr<folly::IOBuf> body) noexcept
{
    auto evb = folly::EventBaseManager::get()->getEventBase();

    const auto bodyBytes = body->length();

    doneBytes += bodyBytes;
    putRemaining -= bodyBytes;

    if(!onBody(body->data(), body->length()))
    {
        ResponseBuilder(downstream_)
            .status(500, "Internal error")
            .body("Write ext error")
            .sendWithEOM();
        finished = true;
        return;
    }
}

bool ApiHandler::onBody(const uint8_t* data, size_t dataSize)
{
    body.append(reinterpret_cast<const char*>(data), dataSize);
    return true;
}

void ApiHandler::onEOM() noexcept
{
    auto evb = folly::EventBaseManager::get()->getEventBase();

    if(finished)
        return;

    if(putRemaining!=0)
    {
        ResponseBuilder(downstream_)
            .status(500, "Internal error")
            .body("Expecting more data")
            .sendWithEOM();
        finished = true; 
        return;
    }

    folly::getGlobalCPUExecutor()->add(
        [this, evb]()
        {
            const auto response = runRequest();

            evb->runInEventBaseThread([this, response]()
                                        {
                auto statusMesg = response.code == 200 ? "OK" : "Internal error";
                
                ResponseBuilder respBuilder(downstream_);
                respBuilder.status(response.code, statusMesg);
                respBuilder.body(response.contentType ? response.body : escapeXML(response.body));

                if(response.setCookie)
                {
                    respBuilder.header(HTTPHeaderCode::HTTP_HEADER_SET_COOKIE, fmt::format("ses={}; SameSite=Strict; HttpOnly", *response.setCookie));
                }

                if(response.contentType)
                {
                    respBuilder.header(HTTPHeaderCode::HTTP_HEADER_CONTENT_TYPE, *response.contentType);
                }

                respBuilder.sendWithEOM();

                finished = true; });
        }
    );
}

void ApiHandler::onUpgrade(UpgradeProtocol /*protocol*/) noexcept
{
    // handler doesn't support upgrades
}

void ApiHandler::requestComplete() noexcept
{
    finished = true;
    self.reset();
}

void ApiHandler::onError(proxygen::ProxygenError) noexcept
{
    finished = true;
    self.reset();
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
            if(authHeader.empty())
                throw ApiError(Api::Herror::sessionRequired);

            auto session = getSession(authHeader, cookieSes);
            if(!session)
                throw ApiError(Api::Herror::sessionNotFound);

            if(body.empty())
                body = "{}";

            auto params = json::parse(body);

            if(func=="logout")
            {
                resp = logout(params, session);
            }
            else
            {
                auto itFunc = ApiFunctions::apiFunctions.find(frozen::string(func));
                assert(itFunc!=ApiFunctions::apiFunctions.end());
                if(itFunc==ApiFunctions::apiFunctions.end())               
                    throw ApiError(Api::Herror::noSuchFunction);
                
                resp = itFunc->second(*this, params, *session);
            }
        }
        
    }
    catch(const json::exception& e)
    {
        return ApiResponse{.code=500, .body=fmt::format("JSON exception id {}: {}", e.id, e.what())};
    }
    catch(const ApiError& e)
    {
        json resp = e.response();
        if( e.response().herror == Api::Herror::sessionNotFound ||
             e.response().herror == Api::Herror::sessionRequired)
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
    if(!isAuthorized("arn:hs5:users", Action::CreateUser, sessionStorage.userId))
        throw ApiError(Api::Herror::accessDenied);

    if(dao.getUserByName(params.username).exists)
        throw ApiError(Api::Herror::userAlreadyExists, fmt::format("User {} already exists", params.username));

    const auto userId = dao.addUser(params.username, 0, getEncodedPassword(params.password), 0);

    if(!userId)
        throw ApiError(Api::Herror::internalDbError, fmt::format("Error adding user with name {}", params.username));

    refreshAuthCache();

    Api::AddUserResp resp;
    resp.id = sfs.encrypt_id(userId.value());

    return resp;
}

std::pair<Api::LoginResp, std::string> ApiHandler::login(const Api::LoginParams& params)
{
    auto user = dao.getUserByName(params.username);
    if(!user.exists)
        throw ApiError(Api::Herror::userNotFound, fmt::format("User {} not found", params.username));

    verifyPassword(user, params.password);

    char jsSes[16];
    folly::Random::secureRandom(jsSes, sizeof(jsSes));
    char cookieSes[16];
    folly::Random::secureRandom(cookieSes, sizeof(cookieSes));
    char accessKey[16];
    folly::Random::secureRandom(accessKey, sizeof(accessKey));
    char secretAccessKey[16];
    folly::Random::secureRandom(secretAccessKey, sizeof(secretAccessKey));

    Api::LoginResp resp;
    resp.ses = folly::hexlify(folly::StringPiece(jsSes, sizeof(jsSes)));

    const auto cookieSesHex = folly::hexlify(folly::StringPiece(cookieSes, sizeof(cookieSes)));
    resp.accessKey = folly::hexlify(folly::StringPiece(accessKey, sizeof(accessKey)));
    resp.secretAccessKey = folly::hexlify(folly::StringPiece(secretAccessKey, sizeof(secretAccessKey)));

    ApiSessionStorage apiSessionStorage{.userId = user.id};
    newSession(resp.ses, cookieSesHex, resp.accessKey, resp.secretAccessKey, std::move(apiSessionStorage));

    return std::make_pair(resp, cookieSesHex);
}

namespace
{

template<typename T, typename U>
std::optional<std::string> listBucket(SingleFileStorage& sfs, const std::string& iterStartVal, const int64_t bucketId, T stopFunc, U outputFunc)
{
    sfs.list_commit();

    SingleFileStorage::IterData iterData = {};
    if(!sfs.iter_start(iterStartVal, false, iterData))
        throw ApiError(Api::Herror::errorStartingListing);

    auto guard = folly::makeGuard([&] { sfs.iter_stop(iterData); });

    bool returnNext = false;

    while(true)
    {
        std::string keyBin, md5sum;
        int64_t offset, size, last_modified;
        std::vector<SingleFileStorage::SPunchItem> extra_exts;
        if(!sfs.iter_curr_val(keyBin, offset, size, extra_exts, last_modified, md5sum, iterData))
            return {};

        const auto keyInfo = extractKeyInfoView(keyBin);

        if(keyInfo.bucketId != bucketId)
            return {};

        if(stopFunc(keyInfo))
            return {};    

        if(returnNext)
            return std::string(keyInfo.key); 

        if(!outputFunc(keyInfo, offset, size, extra_exts, last_modified, md5sum, keyBin))
        {
            returnNext = true;
        }
        
        if(!sfs.iter_next(iterData))
            throw ApiError(Api::Herror::errorListingInIteration);
    }
}

bool bucketEmpty(SingleFileStorage& sfs, int64_t bucketId)
{
    bool hasObjects = false;
    std::ignore = listBucket(sfs, make_key({.key = "", .version=std::numeric_limits<int64_t>::max(), .bucketId = bucketId}),
        bucketId, [](const auto&){ return false; }, [&](const auto& keyInfo, int64_t, int64_t, const auto&, int64_t, const std::string&, const std::string&)
        {
            hasObjects = true;
            return false;
        });
     return !hasObjects;
}

}

Api::ListResp ApiHandler::list(const Api::ListParams& params, const ApiSessionStorage& sessionStorage)
{
    if(params.path.empty() || params.path[0]!='/')
        throw ApiError(Api::Herror::invalidPath);
    if(params.path=="/")
        return listBuckets(params, sessionStorage);

    const auto& path = params.path;
    const size_t bucketNameEnd = path.find('/', 1);

    if(!isAuthorized("arn:aws:s3:::"+path, Action::ListObjects, sessionStorage.userId))
        throw ApiError(Api::Herror::accessDenied);

    const auto bucketName = bucketNameEnd==std::string::npos ? path : path.substr(1, bucketNameEnd);
    const auto prefix = bucketNameEnd==std::string::npos ? std::string() : path.substr(bucketNameEnd+1);

    if(!prefix.empty() && prefix[prefix.size()-1]!='/')
        throw ApiError(Api::Herror::invalidPath);

    const auto bucketIdOpt = buckets::getBucket(bucketName);
    if(!bucketIdOpt)
        throw ApiError(Api::Herror::bucketNotFound);
    const auto bucketId = *bucketIdOpt;
    const auto iterStartVal = make_key({.key = params.continuationToken ? *params.continuationToken : prefix, .version=std::numeric_limits<int64_t>::max(), .bucketId = bucketId});

    Api::ListResp resp;
    resp.isTruncated = true;
    std::string lastOutputKeyStr;

    const auto nextMarker = listBucket(sfs, iterStartVal, bucketId, [&](const auto& keyInfo)
    {
        if(!keyInfo.key.starts_with(prefix))
            return true;

        return false;
    },
    [&](const auto& keyInfo, int64_t offset, int64_t size, const auto& extra_exts, 
        int64_t last_modified, const std::string& md5sum, const std::string& keyBin)
    {
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

        return resp.objects.size()<1000;
    });

    if(nextMarker)
    {
        resp.isTruncated = true;
        resp.nextMarker = *nextMarker;
    }

    return resp;
}

Api::ListResp ApiHandler::listBuckets(const Api::ListParams& params, const ApiSessionStorage& sessionStorage)
{
    if(params.continuationToken)
        throw ApiError(Api::Herror::unexpectedContinuationToken);

    Api::ListResp resp;

    auto bucketNames = buckets::getBucketNames();
    for(auto& obj : bucketNames.objects)
    {
        if(!isAuthorized("arn:aws:s3:::"+obj.name, Action::ListBuckets, sessionStorage.userId))
            continue;
        resp.objects.emplace_back(std::move(obj));
    }
    return resp;
}

Api::AddBucketResp ApiHandler::addBucket(const Api::AddBucketParams& params, const ApiSessionStorage& sessionStorage)
{
    if(params.bucketName.empty())
        throw ApiError(Api::Herror::invalidParameters);;

    if(!isAuthorized("arn:aws:s3:::buckets", Action::CreateBucket, sessionStorage.userId))
        throw ApiError(Api::Herror::accessDenied);

    if(!buckets::isValidBucketName(params.bucketName))
        throw ApiError(Api::Herror::invalidBucketName, fmt::format("Bucket name {} is not valid", params.bucketName));

    const auto bucketId = buckets::addBucket(params.bucketName, true);
    if(bucketId<0)
        throw ApiError(Api::Herror::bucketAlreadyExists);

    return {};
}

Api::StatsResp ApiHandler::stats(const Api::StatsParams& params, const ApiSessionStorage& sessionStorage)
{
    const auto size = sfs.get_data_file_size();
    const auto free = sfs.get_free_space_in_data_file();
    const auto sizeFull = sfs.get_data_file_size_full();
    return Api::StatsResp{.freeSpace=free, .size=size, .sizeFull = sizeFull, .used = size - free};
}

Api::RemoveUserResp ApiHandler::removeUser(const Api::RemoveUserParams& params, const ApiSessionStorage& sessionStorage)
{
    if(params.username == "root")
        throw ApiError(Api::Herror::accessDenied, "Cannot delete root user");
    if(!isAuthorized("arn:hs5:user:::"+params.username, Action::RemoveUser, sessionStorage.userId))
        throw ApiError(Api::Herror::accessDenied);

    auto user = dao.getUserByName(params.username);
    if(!user.exists)
        throw ApiError(Api::Herror::userNotFound, fmt::format("User {} not found", params.username));
    
    dao.deleteUser(user.id);
    if(dao.getDb().getLastChanges() != 1)
        throw ApiError(Api::Herror::internalDbError, fmt::format("Error deleting user {}", params.username));

    refreshAuthCache();
    
    return {};
}

Api::ListUsersResp ApiHandler::listUsers(const Api::ListUsersParams& params, const ApiSessionStorage& sessionStorage)
{
    if(!isAuthorized("arn:hs5:users", Action::ListUsers, sessionStorage.userId))
        throw ApiError(Api::Herror::accessDenied);

    auto users = dao.getUsers();
    Api::ListUsersResp resp;
    resp.isTruncated = false;
    resp.nextMarker = "";
    for(const auto& user : users)
    {
        Api::User item;
        item.id = sfs.encrypt_id(user.id);
        item.username = user.name;
        item.system = user.system == 1;
        resp.users.push_back(std::move(item));
    }
    return resp;
}

Api::AddAccessKeyResp ApiHandler::addAccessKey(const Api::AddAccessKeyParams& params, const ApiSessionStorage& sessionStorage)
{
    const auto userId = sfs.decrypt_id(params.userId);
    const auto user = dao.getUserById(userId);
    if(!user)
        throw ApiError(Api::Herror::accessDenied);    

    if(!isAuthorized("arn:hs5:user:::" + user->name, Action::AddAccessKey, sessionStorage.userId))
        throw ApiError(Api::Herror::accessDenied);

    char accessKey[10];
    folly::Random::secureRandom(accessKey, sizeof(accessKey));
    char secretAccessKey[16];
    folly::Random::secureRandom(secretAccessKey, sizeof(secretAccessKey));

    const auto accessKeyHex = folly::hexlify(folly::StringPiece(accessKey, sizeof(accessKey)));
    const auto secretAccessKeyHex = folly::hexlify(folly::StringPiece(secretAccessKey, sizeof(secretAccessKey)));

    dao.addAccessKey(user->id, "", accessKeyHex, secretAccessKeyHex, 0);

    if(dao.getDb().getLastChanges() != 1)
        throw ApiError(Api::Herror::internalDbError, fmt::format("Error adding access key for user {}", user->name));
        
    refreshAuthCache();

    Api::AddAccessKeyResp resp;
    resp.accessKey = accessKeyHex;
    resp.secretKey = secretAccessKeyHex;
    return resp;
}

Api::ListAccessKeysResp ApiHandler::listAccessKeys(const Api::ListAccessKeysParams& params, const ApiSessionStorage& sessionStorage)
{
    const auto userId = sfs.decrypt_id(params.userId);
    const auto user = dao.getUserById(userId);
    if(!user)
        throw ApiError(Api::Herror::accessDenied);    

    if(!isAuthorized("arn:hs5:user:::" + user->name, Action::ListAccessKeys, sessionStorage.userId))
        throw ApiError(Api::Herror::accessDenied);

    auto accessKeys = dao.getAccessKeysOfUser(user->id);
    Api::ListAccessKeysResp resp;
    resp.isTruncated = false;
    resp.nextMarker = "";
    for(const auto& ak : accessKeys)
    {
        Api::AccessKey item;
        item.id = sfs.encrypt_id(ak.id);
        item.accessKeyId = ak.key;
        resp.accessKeys.push_back(std::move(item));
    }
    return resp;
}

Api::RemoveAccessKeyResp ApiHandler::removeAccessKey(const Api::RemoveAccessKeyParams& params, const ApiSessionStorage& sessionStorage)
{
    const auto accessKeyId = sfs.decrypt_id(params.id);

    const auto accessKey = dao.getAccessKey(accessKeyId);
    if(!accessKey)
        throw ApiError(Api::Herror::accessDenied);

    const auto user = dao.getUserById(accessKey->user_id);
    if(!user)
        throw ApiError(Api::Herror::internalDbError, fmt::format("User with id {} not found", accessKey->user_id));

    if(!isAuthorized("arn:hs5:user:::" + user->name, Action::RemoveAccessKey, sessionStorage.userId))
        throw ApiError(Api::Herror::accessDenied);

    dao.removeAccessKey(accessKeyId);

    if(dao.getDb().getLastChanges() != 1)
        throw ApiError(Api::Herror::internalDbError, fmt::format("Error deleting access key with id {}", params.id));

    refreshAuthCache();

    return {};
}

namespace
{
    void addStatementsToDb(DbDao& dao, const Policy& policy, const int64_t policyId)
    {      
        for(const auto& statement : policy.statements)
        {
            const auto statementId = dao.addPolicyStatement(policyId, statement.sid, static_cast<int>(statement.effect));

            if(dao.getDb().getLastChanges() != 1)
                throw ApiError(Api::Herror::internalDbError, fmt::format("Error adding policy statement with sid {}", statement.sid));

            for(const auto& action : statement.actions)
            {
                dao.addPolicyStatementAction(statementId, static_cast<int>(action));

                if(dao.getDb().getLastChanges() != 1)
                    throw ApiError(Api::Herror::internalDbError, fmt::format("Error adding policy statement action {}", static_cast<int>(action)));
            }

            for(const auto& resource : statement.resources)
            {
                dao.addPolicyStatementResource(statementId, resource);

                if(dao.getDb().getLastChanges() != 1)
                    throw ApiError(Api::Herror::internalDbError, fmt::format("Error adding policy statement resource {}", resource));
            }
        }
    }
}

Api::AddPolicyResp ApiHandler::addPolicy(const Api::AddPolicyParams& params, const ApiSessionStorage& sessionStorage)
{
    if(!isAuthorized("arn:hs5:policies", Action::AddPolicy, sessionStorage.userId))
        throw ApiError(Api::Herror::accessDenied);

    try
    {
        Policy newPolicy(params.policyDocument);

        sqlgen::ScopedManualCommitWriteTransaction trans(&dao.getDb());

        const auto policyId = dao.addPolicy(params.policyName, "", 0, params.policyDocument, 0);

        if(!policyId)
            throw ApiError(Api::Herror::internalDbError, fmt::format("Error adding policy with name {}", params.policyName));

        addStatementsToDb(dao, newPolicy, policyId.value());

        trans.commit();

        refreshAuthCache();

        Api::AddPolicyResp resp;

        resp.id = sfs.encrypt_id(policyId.value());
        return resp;
    }
    catch(const PolicyParseError& e)
    {
        throw ApiError(Api::Herror::invalidPolicyDocument, fmt::format("Error parsing policy document: {}", e.what()));
    }   
}

Api::RemovePolicyResp ApiHandler::removePolicy(const Api::RemovePolicyParams& params, const ApiSessionStorage& sessionStorage)
{
    const auto policyId = sfs.decrypt_id(params.id);
    const auto policy = dao.getPolicy(policyId);
    if(!policy)
        throw ApiError(Api::Herror::accessDenied);
    if(!isAuthorized("arn:hs5:policy:::"+policy->name, Action::RemovePolicy, sessionStorage.userId))
        throw ApiError(Api::Herror::accessDenied);

    dao.removePolicy(policyId);

    if(dao.getDb().getLastChanges() != 1)
        throw ApiError(Api::Herror::internalDbError, fmt::format("Error deleting policy with id {}", params.id));

    refreshAuthCache();

    return {};
}

Api::ListPoliciesResp ApiHandler::listPolicies(const Api::ListPoliciesParams& params, const ApiSessionStorage& sessionStorage)
{
    if(!isAuthorized("arn:hs5:policies", Action::ListPolicies, sessionStorage.userId))
        throw ApiError(Api::Herror::accessDenied);

    auto policies = dao.getPolicies();
    Api::ListPoliciesResp resp;
    resp.isTruncated = false;
    resp.nextMarker = "";
    for(const auto& policy : policies)
    {
        Api::Policy item;
        item.id = sfs.encrypt_id(policy.id);
        item.name = policy.name;
        item.document = policy.data;
        item.system = policy.system == 1;
        resp.policies.push_back(std::move(item));
    }
    return resp;
}

Api::AddRoleResp ApiHandler::addRole(const Api::AddRoleParams& params, const ApiSessionStorage& sessionStorage)
{
    if(!isAuthorized("arn:hs5:roles", Action::AddRole, sessionStorage.userId))
        throw ApiError(Api::Herror::accessDenied);

    const auto roleId = dao.addRole(params.roleName, 0);
    
    if (!roleId)
        throw ApiError(Api::Herror::internalDbError, fmt::format("Error adding role with name {}", params.roleName));

    refreshAuthCache();

    Api::AddRoleResp resp;
    resp.id = sfs.encrypt_id(roleId.value());
    return resp;
}

Api::RemoveRoleResp ApiHandler::removeRole(const Api::RemoveRoleParams& params, const ApiSessionStorage& sessionStorage)
{
    const auto roleId = sfs.decrypt_id(params.id);
    const auto role = dao.getRole(roleId);
    if(!role)
        throw ApiError(Api::Herror::accessDenied);
    if(!isAuthorized("arn:hs5:role:::"+role->name, Action::RemoveRole, sessionStorage.userId))
        throw ApiError(Api::Herror::accessDenied);

    dao.removeRole(roleId);

    if(dao.getDb().getLastChanges() != 1)
        throw ApiError(Api::Herror::internalDbError, fmt::format("Error deleting role with id {}", params.id));

    refreshAuthCache();

    return {};
}

Api::ListRolesResp ApiHandler::listRoles(const Api::ListRolesParams& params, const ApiSessionStorage& sessionStorage)
{
    if(!isAuthorized("arn:hs5:roles", Action::ListRoles, sessionStorage.userId))
        throw ApiError(Api::Herror::accessDenied);

    auto roles = dao.getRoles();
    Api::ListRolesResp resp;
    resp.isTruncated = false;
    resp.nextMarker = "";
    for(const auto& role : roles)
    {
        Api::Role item;
        item.id = sfs.encrypt_id(role.id);
        item.roleName = role.name;
        item.system = role.system == 1;
        resp.roles.push_back(std::move(item));
    }
    return resp;
}

Api::AddUserRoleResp ApiHandler::addUserRole(const Api::AddUserRoleParams& params, const ApiSessionStorage& sessionStorage)
{
    const auto userId = sfs.decrypt_id(params.userId);
    const auto roleId = sfs.decrypt_id(params.roleId);
    
    auto user = dao.getUserById(userId);
    if(!user)
        throw ApiError(Api::Herror::accessDenied);

    const auto role = dao.getRole(roleId);
    if(!role)
        throw ApiError(Api::Herror::accessDenied);

    if(!isAuthorized("arn:hs5:user:::"+user->name, Action::AddUserRole, sessionStorage.userId))
        throw ApiError(Api::Herror::accessDenied);
    if(!isAuthorized("arn:hs5:role:::"+role->name, Action::AddUserRole, sessionStorage.userId))
        throw ApiError(Api::Herror::accessDenied);

    dao.addUserRole(userId, roleId, 0);

    if(dao.getDb().getLastChanges() != 1)
        throw ApiError(Api::Herror::internalDbError, fmt::format("Error adding user role with user id {} and role id {}", params.userId, params.roleId));

    refreshAuthCache();

    return {};
}

Api::RemoveUserRoleResp ApiHandler::removeUserRole(const Api::RemoveUserRoleParams& params, const ApiSessionStorage& sessionStorage)
{
    const auto roleId = sfs.decrypt_id(params.id);

    const auto userRole = dao.getUserRole(roleId);
    if(!userRole)
        throw ApiError(Api::Herror::accessDenied);
    const auto role = dao.getRole(userRole->role_id);
    if(!role)
        throw ApiError(Api::Herror::accessDenied);
    if(!isAuthorized("arn:hs5:role:::"+role->name, Action::RemoveUserRole, sessionStorage.userId))
        throw ApiError(Api::Herror::accessDenied);

    dao.removeUserRole(roleId);

    if (dao.getDb().getLastChanges() != 1)
        throw ApiError(Api::Herror::internalDbError, fmt::format("Error deleting user role with id {}", params.id));

    refreshAuthCache();

    return {};
}

Api::ListUserRolesResp ApiHandler::listUserRoles(const Api::ListUserRolesParams& params, const ApiSessionStorage& sessionStorage)
{
    const auto userId = sfs.decrypt_id(params.userId);

    auto user = dao.getUserById(userId);
    if(!user)
        throw ApiError(Api::Herror::accessDenied);
    if(!isAuthorized("arn:hs5:user:::"+user->name, Action::ListUserRoles, sessionStorage.userId))
        throw ApiError(Api::Herror::accessDenied);
    if(!isAuthorized("arn:hs5:roles", Action::ListUserRoles, sessionStorage.userId))
        throw ApiError(Api::Herror::accessDenied);

    auto userRoles = dao.getRolesByUserId(userId);
    
    Api::ListUserRolesResp resp;
    resp.isTruncated = false;
    resp.nextMarker = "";
    for(const auto& role : userRoles)
    {
        Api::UserRole item;
        item.id = sfs.encrypt_id(role.id);
        item.roleId = sfs.encrypt_id(role.role_id);
        item.roleName = role.name;
        item.system = role.system == 1;
        resp.userRoles.push_back(std::move(item));
    }
    return resp;
}

Api::AddRolePolicyResp ApiHandler::addRolePolicy(const Api::AddRolePolicyParams& params, const ApiSessionStorage& sessionStorage)
{
    const auto roleId = sfs.decrypt_id(params.roleId);
    const auto policyId = sfs.decrypt_id(params.policyId);

    const auto role = dao.getRole(roleId);
    if(!role)
        throw ApiError(Api::Herror::accessDenied);
    if(!isAuthorized("arn:hs5:role:::"+role->name, Action::AddRolePolicy, sessionStorage.userId))
        throw ApiError(Api::Herror::accessDenied);
    
    const auto policy = dao.getPolicy(policyId);
    if(!policy)
        throw ApiError(Api::Herror::accessDenied);
    if(!isAuthorized("arn:hs5:policy:::"+policy->name, Action::AddRolePolicy, sessionStorage.userId))
        throw ApiError(Api::Herror::accessDenied);

    dao.addRolePolicy(roleId, policyId, 0);

    if(dao.getDb().getLastChanges() != 1)
        throw ApiError(Api::Herror::internalDbError, fmt::format("Error adding role policy with role id {} and policy id {}", params.roleId, params.policyId));


    refreshAuthCache();

    return {};
}

Api::RemoveRolePolicyResp ApiHandler::removeRolePolicy(const Api::RemoveRolePolicyParams& params, const ApiSessionStorage& sessionStorage)
{
    const auto rolePolicyId = sfs.decrypt_id(params.id);

    const auto rolePolicy = dao.getRolePolicyById(rolePolicyId);
    if(!rolePolicy)
        throw ApiError(Api::Herror::accessDenied);

    const auto role = dao.getRole(rolePolicy->role_id);
    if(!role)
        throw ApiError(Api::Herror::accessDenied);
    if(!isAuthorized("arn:hs5:role:::"+role->name, Action::RemoveRolePolicy, sessionStorage.userId))
        throw ApiError(Api::Herror::accessDenied);
    
    const auto policy = dao.getPolicy(rolePolicy->policy_id);
    if(!policy)
        throw ApiError(Api::Herror::accessDenied);
    if(!isAuthorized("arn:hs5:policy:::"+policy->name, Action::RemoveRolePolicy, sessionStorage.userId))
        throw ApiError(Api::Herror::accessDenied);

    dao.removeRolePolicy(rolePolicyId);
    
    if (dao.getDb().getLastChanges() != 1)
        throw ApiError(Api::Herror::internalDbError, fmt::format("Error deleting role policy with id {}", params.id)); 

    return {};
}

Api::ListRolePoliciesResp ApiHandler::listRolePolicies(const Api::ListRolePoliciesParams& params, const ApiSessionStorage& sessionStorage)
{
    const auto roleId = sfs.decrypt_id(params.roleId);

    const auto role = dao.getRole(roleId);
    if(!role)
        throw ApiError(Api::Herror::accessDenied);

    if(!isAuthorized("arn:hs5:role:::"+role->name, Action::ListRolePolicies, sessionStorage.userId))
        throw ApiError(Api::Herror::accessDenied);

    if(!isAuthorized("arn:hs5:policies", Action::ListRolePolicies, sessionStorage.userId))
        throw ApiError(Api::Herror::accessDenied);

    auto policies = dao.getPoliciesOfRole(roleId);
    
    Api::ListRolePoliciesResp resp;
    resp.isTruncated = false;
    resp.nextMarker = "";
    for(const auto& policy : policies)
    {
        Api::RolePolicy item;
        item.id = sfs.encrypt_id(policy.id);
        item.policyId = sfs.encrypt_id(policy.policy_id);
        item.policyName = policy.name;
        item.system = policy.system == 1;
        resp.rolePolicies.push_back(std::move(item));
    }
    return resp;
}

Api::ChangePolicyResp ApiHandler::changePolicy(const Api::ChangePolicyParams& params, const ApiSessionStorage& sessionStorage)
{
    const auto policyId = sfs.decrypt_id(params.id);
    const auto policy = dao.getPolicy(policyId);
    if(!policy)
        throw ApiError(Api::Herror::accessDenied);

    if(!isAuthorized("arn:hs5:policy:::"+policy->name, Action::ChangePolicy, sessionStorage.userId))
        throw ApiError(Api::Herror::accessDenied);

    try
    {
        Policy newPolicy(params.document);

        sqlgen::ScopedManualCommitWriteTransaction trans(&dao.getDb());

        dao.changePolicy(params.document, policyId);

        if(dao.getDb().getLastChanges() != 1)
            throw ApiError(Api::Herror::internalDbError, fmt::format("Error changing policy with id {}", params.id));

        dao.deletePolicyStatements(policyId);
           
        addStatementsToDb(dao, newPolicy, policyId);

        trans.commit();
    }
    catch(const PolicyParseError& e)
    {
        throw ApiError(Api::Herror::invalidPolicyDocument, fmt::format("Error parsing policy document: {}", e.what()));
    }
    

    refreshAuthCache();

    return {};
}

Api::DeleteBucketResp ApiHandler::deleteBucket(const Api::DeleteBucketParams& params, const ApiSessionStorage& sessionStorage)
{
    const auto bucketIdOpt = buckets::getBucket(params.bucketName);
    if(!bucketIdOpt)
        throw ApiError(Api::Herror::bucketNotFound);

    const auto bucketId = *bucketIdOpt;

    if(!isAuthorized("arn:aws:s3:::"+params.bucketName, Action::DeleteBucket, sessionStorage.userId))
        throw ApiError(Api::Herror::accessDenied);

    bool hasObjects = false;
    bool bucketDeleted = false;

    do
    {
        hasObjects = false;

        std::mutex delQueueMutex;
        std::condition_variable delQueueCv;
        std::queue<std::string> delQueue;
        std::string delErrorKeyBin;
        int delErrorCode = 0;
        size_t delCount = 0;

        std::jthread delThread([&]() {

            while(true)
            {
                std::string keyBin;

                {
                    std::unique_lock lock(delQueueMutex);
                    delQueueCv.wait(lock, [&] { return !delQueue.empty(); });

                    if(delQueue.empty())
                        break;

                    keyBin = std::move(delQueue.front());
                    delQueue.pop();
                }

                if(keyBin.empty())
                    break;

                auto res = sfs.del(keyBin, SingleFileStorage::DelAction::Del, false);
                if(res!=0)
                {
                    std::scoped_lock lock(delQueueMutex);
                    delErrorCode = res;
                    delErrorKeyBin = std::move(keyBin);
                    break;
                }
            }
        });

        std::ignore = listBucket(sfs, make_key({.key = "", .version=std::numeric_limits<int64_t>::max(), .bucketId = bucketId}),
            bucketId, [](const auto&){ return false; }, [&](const auto& keyInfo, int64_t, int64_t, 
            const auto&, int64_t, const std::string&, const std::string& keyBin)
        {
            hasObjects = true;

            ++delCount;

            std::unique_lock lock(delQueueMutex);

            if(!delErrorKeyBin.empty())
                throw ApiError(Api::Herror::errorDeletingObject, fmt::format("Error deleting object with key {} in bucket {}: {}", extractKeyInfoView(delErrorKeyBin).key, params.bucketName, delErrorCode));
            
            while(delQueue.size()>=5000)
            {
                lock.unlock();
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
                lock.lock();
            }

            delQueue.push(keyBin);
            delQueueCv.notify_one();

            return true;
        });

        {
            std::scoped_lock lock(delQueueMutex);
            delQueue.push(std::string());
            delQueueCv.notify_one();
        }

        delThread.join();

        if(!delErrorKeyBin.empty())
            throw ApiError(Api::Herror::errorDeletingObject, fmt::format("Error deleting object with key {} in bucket {}: {}", extractKeyInfoView(delErrorKeyBin).key, params.bucketName, delErrorCode));

        XLOGF(INFO, "Deleted {} objects from bucket {}", delCount, params.bucketName);

        if(hasObjects)
        {
            const auto rc = sfs.commit(false, -1, FLAGS_pre_sync_commit);
            if(!rc)
            {
                XLOGF(WARN, "Error committing deletion of objects in bucket {}", params.bucketName);
                throw ApiError(Api::Herror::errorDeletingObject, fmt::format("Error committing deletion of objects in bucket {}", params.bucketName));
            }
        }

        if(!bucketDeleted)
        {
            if(!buckets::deleteBucket(bucketId))
            {
                XLOGF(WARN, "Error deleting bucket {} with id {}", params.bucketName, bucketId);
                throw ApiError(Api::Herror::errorDeletingBucket, fmt::format("Error deleting bucket {}", params.bucketName));
            }
            bucketDeleted = true;
        }

        if(bucketEmpty(sfs, bucketId))
            break;
    
    } while(hasObjects);

    return {};
}

Api::ChangePasswordResp ApiHandler::changePassword(const Api::ChangePasswordParams& params, const ApiSessionStorage& sessionStorage)
{
    auto user = dao.getUserById(sessionStorage.userId);
    if(!user)
        throw ApiError(Api::Herror::accessDenied);

    if(!isAuthorized("arn:hs5:user:::"+user->name, Action::ChangePassword, sessionStorage.userId)
        && !isAuthorized("arn:hs5:users", Action::ChangePassword, sessionStorage.userId))
        throw ApiError(Api::Herror::accessDenied);

    verifyPassword(*user, params.oldPassword);

    dao.changeUserPassword(getEncodedPassword(params.password), sessionStorage.userId);

    if(dao.getDb().getLastChanges() != 1)
        throw ApiError(Api::Herror::internalDbError, "Error changing password");

    refreshAuthCache();

    return {};
}

Api::LogoutResp ApiHandler::logout(const Api::LogoutParams& params, SessionScope& sessionStorage)
{
    sessionStorage.unlock();

    if(!invalidateSession(authHeader, cookieSes))
        throw ApiError(Api::Herror::sessionNotFound);

    return {};
}

Api::ListBucketPermissionsResp ApiHandler::listBucketPermissions(const Api::ListBucketPermissionsParams& params, const ApiSessionStorage& sessionStorage)
{
    const auto bucketInfoOpt = buckets::getBucketInfo(params.bucketName);
    if(!bucketInfoOpt)
        throw ApiError(Api::Herror::bucketNotFound);
    const auto& bucketInfo = bucketInfoOpt.value();

    if(!isAuthorized("arn:aws:s3:::"+params.bucketName, Action::ListBucketPermissions, sessionStorage.userId))
        throw ApiError(Api::Herror::accessDenied);

    const auto permissions = dao.getBucketPermissionsWithUsername(bucketInfo.id);
    
    Api::ListBucketPermissionsResp resp;
    resp.isTruncated = false;
    resp.nextMarker = "";
    for(const auto& perm : permissions)
    {
        Api::BucketPermission item;
        item.id = sfs.encrypt_id(perm.id);
        item.userId = sfs.encrypt_id(perm.user_id);
        item.userName = perm.username;

        const auto permSet = perm.permissions;

        if(permSet & BUCKET_PERMISSION_READ)
            item.permissions.push_back(Api::Permission::read);
        if(permSet & BUCKET_PERMISSION_WRITE)
            item.permissions.push_back(Api::Permission::write);
        if(permSet & BUCKET_PERMISSION_DELETE)
            item.permissions.push_back(Api::Permission::permissionDelete);

        resp.bucketPermissions.push_back(std::move(item));
    }

    if(bucketInfo.publicPerms & BUCKET_PERMISSION_READ)
        resp.publicPermissions.push_back(Api::Permission::read);
    if(bucketInfo.publicPerms & BUCKET_PERMISSION_WRITE)
        resp.publicPermissions.push_back(Api::Permission::write);
    if(bucketInfo.publicPerms & BUCKET_PERMISSION_DELETE)
        resp.publicPermissions.push_back(Api::Permission::permissionDelete);

    return resp;
}

Api::AddBucketPermissionResp ApiHandler::addBucketPermission(const Api::AddBucketPermissionParams& params, const ApiSessionStorage& sessionStorage)
{
    const auto bucketIdOpt = buckets::getBucket(params.bucketName);
    if(!bucketIdOpt)
        throw ApiError(Api::Herror::bucketNotFound);
    const auto bucketId = bucketIdOpt.value();

    const auto userId = sfs.decrypt_id(params.userId);
    const auto user = dao.getUserById(userId);

    if(!user)
        throw ApiError(Api::Herror::userNotFound);

    if(!isAuthorized("arn:aws:s3:::"+params.bucketName, Action::AddBucketPermission, sessionStorage.userId))
        throw ApiError(Api::Herror::accessDenied);

    int perms = 0;

    for(const auto p : params.addBucketPermissions)
    {
        switch(p)
        {
            case Api::AddBucketPermission::read:
                perms |= BUCKET_PERMISSION_READ;
                break;
            case Api::AddBucketPermission::write:
                perms |= BUCKET_PERMISSION_WRITE;
                break;
            case Api::AddBucketPermission::addBucketPermissionDelete:
                perms |= BUCKET_PERMISSION_DELETE;
                break;
            default:
                throw ApiError(Api::Herror::invalidParameters, fmt::format("Invalid permission value {}", static_cast<int>(p)));
        }
    }

    dao.addBucketPermission(bucketId, userId, perms);

    if(dao.getDb().getLastChanges() != 1)
        throw ApiError(Api::Herror::internalDbError, fmt::format("Error adding permission for bucket {}", params.bucketName));

    refreshAuthCache();

    return {};
}

Api::RemoveBucketPermissionResp ApiHandler::removeBucketPermission(const Api::RemoveBucketPermissionParams& params, const ApiSessionStorage& sessionStorage)
{
    const auto permId = sfs.decrypt_id(params.id);

    const auto permOpt = dao.getBucketPermission(permId);

    if(!permOpt)
        throw ApiError(Api::Herror::accessDenied);

    const auto& perm = permOpt.value();

    const auto bucketId = perm.bucket_id;

    const auto bucketName = buckets::getBucketName(bucketId);
    if(bucketName.empty())
        throw ApiError(Api::Herror::bucketNotFound);

    if(!isAuthorized("arn:aws:s3:::"+bucketName, Action::RemoveBucketPermission, sessionStorage.userId))
        throw ApiError(Api::Herror::accessDenied);

    dao.removeBucketPermission(permId);

    if(dao.getDb().getLastChanges() != 1)
        throw ApiError(Api::Herror::internalDbError, fmt::format("Error deleting permission with id {} for bucket {}", permId, bucketName));

    refreshAuthCache();

    return {};
}

Api::SessionCheckResp ApiHandler::sessionCheck(const Api::SessionCheckParams& params, const ApiSessionStorage& sessionStorage)
{
    return {};
}

Api::SetBucketPublicResp ApiHandler::setBucketPublic(const Api::SetBucketPublicParams& params, const ApiSessionStorage& sessionStorage)
{
    const auto bucketIdOpt = buckets::getBucket(params.bucketName);
    if(!bucketIdOpt)
        throw ApiError(Api::Herror::bucketNotFound);
    const auto bucketId = bucketIdOpt.value();

    if(!isAuthorized("arn:aws:s3:::"+params.bucketName, Action::SetBucketPublic, sessionStorage.userId))
        throw ApiError(Api::Herror::accessDenied);

    int perms = 0;

    for(const auto p : params.setBucketPublicParamsPublic)
    {
        switch(p)
        {
            case Api::Public::read:
                perms |= BUCKET_PERMISSION_READ;
                break;
            case Api::Public::write:
                perms |= BUCKET_PERMISSION_WRITE;
                break;
            case Api::Public::publicDelete:
                perms |= BUCKET_PERMISSION_DELETE;
                break;
            default:
                throw ApiError(Api::Herror::invalidParameters, fmt::format("Invalid permission value {}", static_cast<int>(p)));
        }
    }

    dao.setBucketPublicPerms(perms, bucketId);

    if(dao.getDb().getLastChanges() != 1)
        throw ApiError(Api::Herror::internalDbError, fmt::format("Error setting bucket {} public value to {}", params.bucketName, perms));

    buckets::refreshBucketCache();

    return {};
}