//  To parse this JSON data, first install
//
//      json.hpp  https://github.com/nlohmann/json
//
//  Then include this file, and then do
//
//     ListResp.hpp data = nlohmann::json::parse(jsonString);

#pragma once

#include <nlohmann/json.hpp>
#include "helper.hpp"

#include "Object.hpp"

namespace Api {
    using nlohmann::json;

    struct ListResp {
        bool isTruncated;
        std::string nextMarker;
        std::vector<Object> objects;
    };
}
