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

#include "ListRolesResp.hpp"
#include "Role.hpp"

namespace Api {
    void from_json(const json & j, Role & x);
    void to_json(json & j, const Role & x);

    void from_json(const json & j, ListRolesResp & x);
    void to_json(json & j, const ListRolesResp & x);

    inline void from_json(const json & j, Role& x) {
        x.id = j.at("id").get<std::string>();
        x.roleName = j.at("roleName").get<std::string>();
    }

    inline void to_json(json & j, const Role & x) {
        j = json::object();
        j["id"] = x.id;
        j["roleName"] = x.roleName;
    }

    inline void from_json(const json & j, ListRolesResp& x) {
        x.isTruncated = j.at("isTruncated").get<bool>();
        x.nextMarker = j.at("nextMarker").get<std::string>();
        x.roles = j.at("roles").get<std::vector<Role>>();
    }

    inline void to_json(json & j, const ListRolesResp & x) {
        j = json::object();
        j["isTruncated"] = x.isTruncated;
        j["nextMarker"] = x.nextMarker;
        j["roles"] = x.roles;
    }
}
