//  To parse this JSON data, first install
//
//      json.hpp  https://github.com/nlohmann/json
//
//  Then include this file, and then do
//
//     AddBucketResp.hpp data = nlohmann::json::parse(jsonString);

#pragma once

#include <optional>
#include <nlohmann/json.hpp>
#include "helper.hpp"

namespace Api {
    using nlohmann::json;

    struct AddBucketResp {
        std::optional<std::string> dummy;
    };
}
