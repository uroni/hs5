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

#include "ListRolePoliciesResp.hpp"
#include "RolePolicy.hpp"

namespace Api {
    void from_json(const json & j, RolePolicy & x);
    void to_json(json & j, const RolePolicy & x);

    void from_json(const json & j, ListRolePoliciesResp & x);
    void to_json(json & j, const ListRolePoliciesResp & x);

    inline void from_json(const json & j, RolePolicy& x) {
        x.id = j.at("id").get<std::string>();
        x.policyId = j.at("policyId").get<std::string>();
        x.policyName = j.at("policyName").get<std::string>();
    }

    inline void to_json(json & j, const RolePolicy & x) {
        j = json::object();
        j["id"] = x.id;
        j["policyId"] = x.policyId;
        j["policyName"] = x.policyName;
    }

    inline void from_json(const json & j, ListRolePoliciesResp& x) {
        x.isTruncated = j.at("isTruncated").get<bool>();
        x.nextMarker = j.at("nextMarker").get<std::string>();
        x.rolePolicies = j.at("rolePolicies").get<std::vector<RolePolicy>>();
    }

    inline void to_json(json & j, const ListRolePoliciesResp & x) {
        j = json::object();
        j["isTruncated"] = x.isTruncated;
        j["nextMarker"] = x.nextMarker;
        j["rolePolicies"] = x.rolePolicies;
    }
}
