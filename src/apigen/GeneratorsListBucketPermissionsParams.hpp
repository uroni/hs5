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

#include "ListBucketPermissionsParams.hpp"

namespace Api {
    void from_json(const json & j, ListBucketPermissionsParams & x);
    void to_json(json & j, const ListBucketPermissionsParams & x);

    inline void from_json(const json & j, ListBucketPermissionsParams& x) {
        x.bucketName = j.at("bucketName").get<std::string>();
    }

    inline void to_json(json & j, const ListBucketPermissionsParams & x) {
        j = json::object();
        j["bucketName"] = x.bucketName;
    }
}
