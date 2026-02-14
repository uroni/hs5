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

#include "ListUsersResp.hpp"
#include "User.hpp"

namespace Api {
    void from_json(const json & j, User & x);
    void to_json(json & j, const User & x);

    void from_json(const json & j, ListUsersResp & x);
    void to_json(json & j, const ListUsersResp & x);

    inline void from_json(const json & j, User& x) {
        x.username = j.at("username").get<std::string>();
    }

    inline void to_json(json & j, const User & x) {
        j = json::object();
        j["username"] = x.username;
    }

    inline void from_json(const json & j, ListUsersResp& x) {
        x.isTruncated = j.at("isTruncated").get<bool>();
        x.nextMarker = j.at("nextMarker").get<std::string>();
        x.users = j.at("users").get<std::vector<User>>();
    }

    inline void to_json(json & j, const ListUsersResp & x) {
        j = json::object();
        j["isTruncated"] = x.isTruncated;
        j["nextMarker"] = x.nextMarker;
        j["users"] = x.users;
    }
}
