//  To parse this JSON data, first install
//
//      json.hpp  https://github.com/nlohmann/json
//
//  Then include this file, and then do
//
//     ListUserRolesResp.hpp data = nlohmann::json::parse(jsonString);

#pragma once

#include <nlohmann/json.hpp>
#include "helper.hpp"

#include "UserRole.hpp"

namespace Api {
    using nlohmann::json;

    struct ListUserRolesResp {
        bool isTruncated;
        std::string nextMarker;
        std::vector<UserRole> userRoles;
    };
}
