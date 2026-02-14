#pragma once

#include <string>
#include <stdexcept>

#include "DbDao.h"
#include "apigen/AddUserParams.hpp"
#include "apigen/AddUserResp.hpp"
#include "apigen/RemoveUserParams.hpp"
#include "apigen/RemoveUserResp.hpp"
#include "apigen/ListUsersParams.hpp"
#include "apigen/ListUsersResp.hpp"
#include "apigen/AddAccessKeyParams.hpp"
#include "apigen/AddAccessKeyResp.hpp"
#include "apigen/ListAccessKeysParams.hpp"
#include "apigen/ListAccessKeysResp.hpp"
#include "apigen/RemoveAccessKeyParams.hpp"
#include "apigen/RemoveAccessKeyResp.hpp"
#include "apigen/AddPolicyParams.hpp"
#include "apigen/AddPolicyResp.hpp"
#include "apigen/RemovePolicyParams.hpp"
#include "apigen/RemovePolicyResp.hpp"
#include "apigen/ListPoliciesParams.hpp"
#include "apigen/ListPoliciesResp.hpp"
#include "apigen/AddRoleParams.hpp"
#include "apigen/AddRoleResp.hpp"
#include "apigen/RemoveRoleParams.hpp"
#include "apigen/RemoveRoleResp.hpp"
#include "apigen/ListRolesParams.hpp"
#include "apigen/ListRolesResp.hpp"
#include "apigen/AddUserRoleParams.hpp"
#include "apigen/AddUserRoleResp.hpp"
#include "apigen/RemoveUserRoleParams.hpp"
#include "apigen/RemoveUserRoleResp.hpp"
#include "apigen/ListUserRolesParams.hpp"
#include "apigen/ListUserRolesResp.hpp"
#include "apigen/AddRolePolicyParams.hpp"
#include "apigen/AddRolePolicyResp.hpp"
#include "apigen/RemoveRolePolicyParams.hpp"
#include "apigen/RemoveRolePolicyResp.hpp"
#include "apigen/ListRolePoliciesParams.hpp"
#include "apigen/ListRolePoliciesResp.hpp"
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
#include "apigen/ChangePolicyParams.hpp"
#include "apigen/ChangePolicyResp.hpp"
#include "apigen/DeleteBucketParams.hpp"
#include "apigen/DeleteBucketResp.hpp"
#include "apigen/ChangePasswordParams.hpp"
#include "apigen/ChangePasswordResp.hpp"
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
    Api::RemoveUserResp removeUser(const Api::RemoveUserParams& params, const ApiSessionStorage& sessionStorage);
    Api::ListUsersResp listUsers(const Api::ListUsersParams& params, const ApiSessionStorage& sessionStorage);
    Api::AddAccessKeyResp addAccessKey(const Api::AddAccessKeyParams& params, const ApiSessionStorage& sessionStorage);
    Api::ListAccessKeysResp listAccessKeys(const Api::ListAccessKeysParams& params, const ApiSessionStorage& sessionStorage);
    Api::RemoveAccessKeyResp removeAccessKey(const Api::RemoveAccessKeyParams& params, const ApiSessionStorage& sessionStorage);
    Api::AddPolicyResp addPolicy(const Api::AddPolicyParams& params, const ApiSessionStorage& sessionStorage);
    Api::RemovePolicyResp removePolicy(const Api::RemovePolicyParams& params, const ApiSessionStorage& sessionStorage);
    Api::ListPoliciesResp listPolicies(const Api::ListPoliciesParams& params, const ApiSessionStorage& sessionStorage);
    Api::AddRoleResp addRole(const Api::AddRoleParams& params, const ApiSessionStorage& sessionStorage);
    Api::RemoveRoleResp removeRole(const Api::RemoveRoleParams& params, const ApiSessionStorage& sessionStorage);
    Api::ListRolesResp listRoles(const Api::ListRolesParams& params, const ApiSessionStorage& sessionStorage);
    Api::AddUserRoleResp addUserRole(const Api::AddUserRoleParams& params, const ApiSessionStorage& sessionStorage);
    Api::RemoveUserRoleResp removeUserRole(const Api::RemoveUserRoleParams& params, const ApiSessionStorage& sessionStorage);
    Api::ListUserRolesResp listUserRoles(const Api::ListUserRolesParams& params, const ApiSessionStorage& sessionStorage);
    Api::AddRolePolicyResp addRolePolicy(const Api::AddRolePolicyParams& params, const ApiSessionStorage& sessionStorage);
    Api::RemoveRolePolicyResp removeRolePolicy(const Api::RemoveRolePolicyParams& params, const ApiSessionStorage& sessionStorage);
    Api::ListRolePoliciesResp listRolePolicies(const Api::ListRolePoliciesParams& params, const ApiSessionStorage& sessionStorage);
    std::pair<Api::LoginResp, std::string> login(const Api::LoginParams& params);
    Api::ListResp list(const Api::ListParams& params, const ApiSessionStorage& sessionStorage);
    Api::ListResp listBuckets(const Api::ListParams& params, const ApiSessionStorage& sessionStorage);
    Api::AddBucketResp addBucket(const Api::AddBucketParams& params, const ApiSessionStorage& sessionStorage);
    Api::StatsResp stats(const Api::StatsParams& params, const ApiSessionStorage& sessionStorage);
    Api::ChangePolicyResp changePolicy(const Api::ChangePolicyParams& params, const ApiSessionStorage& sessionStorage);
    Api::DeleteBucketResp deleteBucket(const Api::DeleteBucketParams& params, const ApiSessionStorage& sessionStorage);
    Api::ChangePasswordResp changePassword(const Api::ChangePasswordParams& params, const ApiSessionStorage& sessionStorage);

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
