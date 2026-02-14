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

#include "ListRolePoliciesParams.hpp"

namespace Api {
    void from_json(const json & j, ListRolePoliciesParams & x);
    void to_json(json & j, const ListRolePoliciesParams & x);

    inline void from_json(const json & j, ListRolePoliciesParams& x) {
        x.continuationToken = get_stack_optional<std::string>(j, "continuationToken");
        x.roleId = j.at("roleId").get<std::string>();
        x.ses = j.at("ses").get<std::string>();
    }

    inline void to_json(json & j, const ListRolePoliciesParams & x) {
        j = json::object();
        j["continuationToken"] = x.continuationToken;
        j["roleId"] = x.roleId;
        j["ses"] = x.ses;
    }
}
