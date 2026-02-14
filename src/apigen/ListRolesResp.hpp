//  To parse this JSON data, first install
//
//      json.hpp  https://github.com/nlohmann/json
//
//  Then include this file, and then do
//
//     ListRolesResp.hpp data = nlohmann::json::parse(jsonString);

#pragma once

#include <nlohmann/json.hpp>
#include "helper.hpp"

#include "Role.hpp"

namespace Api {
    using nlohmann::json;

    struct ListRolesResp {
        bool isTruncated;
        std::string nextMarker;
        std::vector<Role> roles;
    };
}
