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

#include "AddRoleParams.hpp"

namespace Api {
    void from_json(const json & j, AddRoleParams & x);
    void to_json(json & j, const AddRoleParams & x);

    inline void from_json(const json & j, AddRoleParams& x) {
        x.roleName = j.at("roleName").get<std::string>();
        x.ses = j.at("ses").get<std::string>();
    }

    inline void to_json(json & j, const AddRoleParams & x) {
        j = json::object();
        j["roleName"] = x.roleName;
        j["ses"] = x.ses;
    }
}
