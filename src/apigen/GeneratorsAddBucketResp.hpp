//  To parse this JSON data, first install
//
//      json.hpp  https://github.com/nlohmann/json
//
//  Then include this file, and then do
//
//     Generators.hpp data = nlohmann::json::parse(jsonString);

#pragma once

#include <optional>
#include <nlohmann/json.hpp>
#include "helper.hpp"

#include "AddBucketResp.hpp"

namespace Api {
    void from_json(const json & j, AddBucketResp & x);
    void to_json(json & j, const AddBucketResp & x);

    inline void from_json(const json & j, AddBucketResp& x) {
        x.dummy = get_stack_optional<std::string>(j, "dummy");
    }

    inline void to_json(json & j, const AddBucketResp & x) {
        j = json::object();
        j["dummy"] = x.dummy;
    }
}
