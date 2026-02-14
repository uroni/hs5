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

#include "ListUserRolesParams.hpp"

namespace Api {
    void from_json(const json & j, ListUserRolesParams & x);
    void to_json(json & j, const ListUserRolesParams & x);

    inline void from_json(const json & j, ListUserRolesParams& x) {
        x.continuationToken = get_stack_optional<std::string>(j, "continuationToken");
        x.ses = j.at("ses").get<std::string>();
        x.userId = j.at("userId").get<std::string>();
    }

    inline void to_json(json & j, const ListUserRolesParams & x) {
        j = json::object();
        j["continuationToken"] = x.continuationToken;
        j["ses"] = x.ses;
        j["userId"] = x.userId;
    }
}
