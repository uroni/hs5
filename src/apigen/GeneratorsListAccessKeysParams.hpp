//  To parse this JSON data, first install
//
//      json.hpp  https://github.com/nlohmann/json
//
//  Then include this file, and then do
//
//     Generators.hpp data = nlohmann::json::parse(jsonString);

#pragma once

#include <optional>
#include <nlohmann/json.hpp>
#include "helper.hpp"

#include "ListAccessKeysParams.hpp"

namespace Api {
    void from_json(const json & j, ListAccessKeysParams & x);
    void to_json(json & j, const ListAccessKeysParams & x);

    inline void from_json(const json & j, ListAccessKeysParams& x) {
        x.continuationToken = get_stack_optional<std::string>(j, "continuationToken");
        x.ses = j.at("ses").get<std::string>();
        x.username = j.at("username").get<std::string>();
    }

    inline void to_json(json & j, const ListAccessKeysParams & x) {
        j = json::object();
        j["continuationToken"] = x.continuationToken;
        j["ses"] = x.ses;
        j["username"] = x.username;
    }
}
