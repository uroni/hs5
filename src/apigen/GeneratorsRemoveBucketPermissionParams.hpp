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

#include "RemoveBucketPermissionParams.hpp"

namespace Api {
    void from_json(const json & j, RemoveBucketPermissionParams & x);
    void to_json(json & j, const RemoveBucketPermissionParams & x);

    inline void from_json(const json & j, RemoveBucketPermissionParams& x) {
        x.id = j.at("id").get<std::string>();
    }

    inline void to_json(json & j, const RemoveBucketPermissionParams & x) {
        j = json::object();
        j["id"] = x.id;
    }
}
