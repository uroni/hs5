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

#include "AddUserRoleParams.hpp"

namespace Api {
    void from_json(const json & j, AddUserRoleParams & x);
    void to_json(json & j, const AddUserRoleParams & x);

    inline void from_json(const json & j, AddUserRoleParams& x) {
        x.roleId = j.at("roleId").get<std::string>();
        x.ses = j.at("ses").get<std::string>();
        x.userId = j.at("userId").get<std::string>();
    }

    inline void to_json(json & j, const AddUserRoleParams & x) {
        j = json::object();
        j["roleId"] = x.roleId;
        j["ses"] = x.ses;
        j["userId"] = x.userId;
    }
}
