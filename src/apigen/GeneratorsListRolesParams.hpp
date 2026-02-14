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

#include "ListRolesParams.hpp"

namespace Api {
    void from_json(const json & j, ListRolesParams & x);
    void to_json(json & j, const ListRolesParams & x);

    inline void from_json(const json & j, ListRolesParams& x) {
        x.continuationToken = get_stack_optional<std::string>(j, "continuationToken");
        x.ses = j.at("ses").get<std::string>();
    }

    inline void to_json(json & j, const ListRolesParams & x) {
        j = json::object();
        j["continuationToken"] = x.continuationToken;
        j["ses"] = x.ses;
    }
}
