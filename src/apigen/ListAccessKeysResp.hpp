//  To parse this JSON data, first install
//
//      json.hpp  https://github.com/nlohmann/json
//
//  Then include this file, and then do
//
//     ListAccessKeysResp.hpp data = nlohmann::json::parse(jsonString);

#pragma once

#include <nlohmann/json.hpp>
#include "helper.hpp"

#include "AccessKey.hpp"

namespace Api {
    using nlohmann::json;

    struct ListAccessKeysResp {
        std::vector<AccessKey> accessKeys;
        bool isTruncated;
        std::string nextMarker;
    };
}
