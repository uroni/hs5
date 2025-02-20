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

#include "LoginParams.hpp"

namespace Api {
    void from_json(const json & j, LoginParams & x);
    void to_json(json & j, const LoginParams & x);

    inline void from_json(const json & j, LoginParams& x) {
        x.password = j.at("password").get<std::string>();
        x.username = j.at("username").get<std::string>();
    }

    inline void to_json(json & j, const LoginParams & x) {
        j = json::object();
        j["password"] = x.password;
        j["username"] = x.username;
    }
}
