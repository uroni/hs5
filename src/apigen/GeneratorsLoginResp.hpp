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

#include "LoginResp.hpp"

namespace Api {
    void from_json(const json & j, LoginResp & x);
    void to_json(json & j, const LoginResp & x);

    inline void from_json(const json & j, LoginResp& x) {
        x.accessKey = j.at("accessKey").get<std::string>();
        x.secretAccessKey = j.at("secretAccessKey").get<std::string>();
        x.ses = j.at("ses").get<std::string>();
    }

    inline void to_json(json & j, const LoginResp & x) {
        j = json::object();
        j["accessKey"] = x.accessKey;
        j["secretAccessKey"] = x.secretAccessKey;
        j["ses"] = x.ses;
    }
}
