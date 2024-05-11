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

#include "AddUserParams.hpp"

namespace Api {
    void from_json(const json & j, AddUserParams & x);
    void to_json(json & j, const AddUserParams & x);

    inline void from_json(const json & j, AddUserParams& x) {
        x.password = j.at("password").get<std::string>();
        x.ses = j.at("ses").get<std::string>();
        x.username = j.at("username").get<std::string>();
    }

    inline void to_json(json & j, const AddUserParams & x) {
        j = json::object();
        j["password"] = x.password;
        j["ses"] = x.ses;
        j["username"] = x.username;
    }
}
