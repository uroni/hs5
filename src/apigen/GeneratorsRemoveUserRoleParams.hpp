//  To parse this JSON data, first install
//
//      json.hpp  https://github.com/nlohmann/json
//
//  Then include this file, and then do
//
//     Generators.hpp data = nlohmann::json::parse(jsonString);

#pragma once

#include <nlohmann/json.hpp>
#include "helper.hpp"

#include "RemoveUserRoleParams.hpp"

namespace Api {
    void from_json(const json & j, RemoveUserRoleParams & x);
    void to_json(json & j, const RemoveUserRoleParams & x);

    inline void from_json(const json & j, RemoveUserRoleParams& x) {
        x.id = j.at("id").get<std::string>();
        x.ses = j.at("ses").get<std::string>();
    }

    inline void to_json(json & j, const RemoveUserRoleParams & x) {
        j = json::object();
        j["id"] = x.id;
        j["ses"] = x.ses;
    }
}
