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

#include "RemoveUserParams.hpp"

namespace Api {
    void from_json(const json & j, RemoveUserParams & x);
    void to_json(json & j, const RemoveUserParams & x);

    inline void from_json(const json & j, RemoveUserParams& x) {
        x.ses = j.at("ses").get<std::string>();
        x.username = j.at("username").get<std::string>();
    }

    inline void to_json(json & j, const RemoveUserParams & x) {
        j = json::object();
        j["ses"] = x.ses;
        j["username"] = x.username;
    }
}
