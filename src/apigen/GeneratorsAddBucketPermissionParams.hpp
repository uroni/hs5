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

#include "AddBucketPermissionParams.hpp"
#include "AddBucketPermission.hpp"

namespace Api {
    void from_json(const json & j, AddBucketPermissionParams & x);
    void to_json(json & j, const AddBucketPermissionParams & x);

    void from_json(const json & j, AddBucketPermission & x);
    void to_json(json & j, const AddBucketPermission & x);

    inline void from_json(const json & j, AddBucketPermissionParams& x) {
        x.addBucketPermissions = j.at("addBucketPermissions").get<std::vector<AddBucketPermission>>();
        x.bucketId = j.at("bucketId").get<std::string>();
        x.userId = j.at("userId").get<std::string>();
    }

    inline void to_json(json & j, const AddBucketPermissionParams & x) {
        j = json::object();
        j["addBucketPermissions"] = x.addBucketPermissions;
        j["bucketId"] = x.bucketId;
        j["userId"] = x.userId;
    }

    inline void from_json(const json & j, AddBucketPermission & x) {
        if (j == "delete") x = AddBucketPermission::addBucketPermissionDelete;
        else if (j == "read") x = AddBucketPermission::read;
        else if (j == "write") x = AddBucketPermission::write;
        else { throw std::runtime_error("Input JSON does not conform to schema!"); }
    }

    inline void to_json(json & j, const AddBucketPermission & x) {
        switch (x) {
            case AddBucketPermission::addBucketPermissionDelete: j = "delete"; break;
            case AddBucketPermission::read: j = "read"; break;
            case AddBucketPermission::write: j = "write"; break;
            default: throw std::runtime_error("Unexpected value in enumeration \"AddBucketPermission\": " + std::to_string(static_cast<int>(x)));
        }
    }
}
