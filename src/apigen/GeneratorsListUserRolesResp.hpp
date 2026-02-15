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

#include "ListUserRolesResp.hpp"
#include "UserRole.hpp"

namespace Api {
    void from_json(const json & j, UserRole & x);
    void to_json(json & j, const UserRole & x);

    void from_json(const json & j, ListUserRolesResp & x);
    void to_json(json & j, const ListUserRolesResp & x);

    inline void from_json(const json & j, UserRole& x) {
        x.id = j.at("id").get<std::string>();
        x.roleId = j.at("roleId").get<std::string>();
        x.roleName = j.at("roleName").get<std::string>();
        x.system = j.at("system").get<bool>();
    }

    inline void to_json(json & j, const UserRole & x) {
        j = json::object();
        j["id"] = x.id;
        j["roleId"] = x.roleId;
        j["roleName"] = x.roleName;
        j["system"] = x.system;
    }

    inline void from_json(const json & j, ListUserRolesResp& x) {
        x.isTruncated = j.at("isTruncated").get<bool>();
        x.nextMarker = j.at("nextMarker").get<std::string>();
        x.userRoles = j.at("userRoles").get<std::vector<UserRole>>();
    }

    inline void to_json(json & j, const ListUserRolesResp & x) {
        j = json::object();
        j["isTruncated"] = x.isTruncated;
        j["nextMarker"] = x.nextMarker;
        j["userRoles"] = x.userRoles;
    }
}
