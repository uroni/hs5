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

#include "ChangePasswordParams.hpp"

namespace Api {
    void from_json(const json & j, ChangePasswordParams & x);
    void to_json(json & j, const ChangePasswordParams & x);

    inline void from_json(const json & j, ChangePasswordParams& x) {
        x.oldPassword = j.at("oldPassword").get<std::string>();
        x.password = j.at("password").get<std::string>();
        x.ses = j.at("ses").get<std::string>();
    }

    inline void to_json(json & j, const ChangePasswordParams & x) {
        j = json::object();
        j["oldPassword"] = x.oldPassword;
        j["password"] = x.password;
        j["ses"] = x.ses;
    }
}
