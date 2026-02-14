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

#include "DeleteBucketParams.hpp"

namespace Api {
    void from_json(const json & j, DeleteBucketParams & x);
    void to_json(json & j, const DeleteBucketParams & x);

    inline void from_json(const json & j, DeleteBucketParams& x) {
        x.bucketName = j.at("bucketName").get<std::string>();
        x.ses = j.at("ses").get<std::string>();
    }

    inline void to_json(json & j, const DeleteBucketParams & x) {
        j = json::object();
        j["bucketName"] = x.bucketName;
        j["ses"] = x.ses;
    }
}
