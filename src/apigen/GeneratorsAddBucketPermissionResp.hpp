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

#include "AddBucketPermissionResp.hpp"

namespace Api {
    void from_json(const json & j, AddBucketPermissionResp & x);
    void to_json(json & j, const AddBucketPermissionResp & x);

    inline void from_json(const json & j, AddBucketPermissionResp& x) {
        x.id = j.at("id").get<std::string>();
    }

    inline void to_json(json & j, const AddBucketPermissionResp & x) {
        j = json::object();
        j["id"] = x.id;
    }
}
