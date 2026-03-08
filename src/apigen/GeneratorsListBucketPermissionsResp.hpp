//  To parse this JSON data, first install
//
//      json.hpp  https://github.com/nlohmann/json
//
//  Then include this file, and then do
//
//     Generators.hpp data = nlohmann::json::parse(jsonString);

#pragma once

#include <nlohmann/json.hpp>
#include "helper.hpp"

#include "ListBucketPermissionsResp.hpp"
#include "BucketPermission.hpp"
#include "Permission.hpp"

namespace Api {
    void from_json(const json & j, BucketPermission & x);
    void to_json(json & j, const BucketPermission & x);

    void from_json(const json & j, ListBucketPermissionsResp & x);
    void to_json(json & j, const ListBucketPermissionsResp & x);

    void from_json(const json & j, Permission & x);
    void to_json(json & j, const Permission & x);

    inline void from_json(const json & j, BucketPermission& x) {
        x.id = j.at("id").get<std::string>();
        x.permissions = j.at("permissions").get<std::vector<Permission>>();
        x.userId = j.at("userId").get<std::string>();
        x.userName = j.at("userName").get<std::string>();
    }

    inline void to_json(json & j, const BucketPermission & x) {
        j = json::object();
        j["id"] = x.id;
        j["permissions"] = x.permissions;
        j["userId"] = x.userId;
        j["userName"] = x.userName;
    }

    inline void from_json(const json & j, ListBucketPermissionsResp& x) {
        x.bucketPermissions = j.at("bucketPermissions").get<std::vector<BucketPermission>>();
        x.isTruncated = j.at("isTruncated").get<bool>();
        x.nextMarker = j.at("nextMarker").get<std::string>();
    }

    inline void to_json(json & j, const ListBucketPermissionsResp & x) {
        j = json::object();
        j["bucketPermissions"] = x.bucketPermissions;
        j["isTruncated"] = x.isTruncated;
        j["nextMarker"] = x.nextMarker;
    }

    inline void from_json(const json & j, Permission & x) {
        if (j == "delete") x = Permission::permissionDelete;
        else if (j == "read") x = Permission::read;
        else if (j == "write") x = Permission::write;
        else { throw std::runtime_error("Input JSON does not conform to schema!"); }
    }

    inline void to_json(json & j, const Permission & x) {
        switch (x) {
            case Permission::permissionDelete: j = "delete"; break;
            case Permission::read: j = "read"; break;
            case Permission::write: j = "write"; break;
            default: throw std::runtime_error("Unexpected value in enumeration \"Permission\": " + std::to_string(static_cast<int>(x)));
        }
    }
}
