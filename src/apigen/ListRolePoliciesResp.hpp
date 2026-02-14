//  To parse this JSON data, first install
//
//      json.hpp  https://github.com/nlohmann/json
//
//  Then include this file, and then do
//
//     ListRolePoliciesResp.hpp data = nlohmann::json::parse(jsonString);

#pragma once

#include <nlohmann/json.hpp>
#include "helper.hpp"

#include "RolePolicy.hpp"

namespace Api {
    using nlohmann::json;

    struct ListRolePoliciesResp {
        bool isTruncated;
        std::string nextMarker;
        std::vector<RolePolicy> rolePolicies;
    };
}
