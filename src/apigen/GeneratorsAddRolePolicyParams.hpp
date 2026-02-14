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

#include "AddRolePolicyParams.hpp"

namespace Api {
    void from_json(const json & j, AddRolePolicyParams & x);
    void to_json(json & j, const AddRolePolicyParams & x);

    inline void from_json(const json & j, AddRolePolicyParams& x) {
        x.policyId = j.at("policyId").get<std::string>();
        x.roleId = j.at("roleId").get<std::string>();
        x.ses = j.at("ses").get<std::string>();
    }

    inline void to_json(json & j, const AddRolePolicyParams & x) {
        j = json::object();
        j["policyId"] = x.policyId;
        j["roleId"] = x.roleId;
        j["ses"] = x.ses;
    }
}
